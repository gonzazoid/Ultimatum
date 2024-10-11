// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "base/task/sequenced_task_runner.h"
#include "net/base/io_buffer.h"
#include "net/disk_cache_raw_api/put_entry.h"

namespace disk_cache {

  CacheStorageRawApiPutEntry::CacheStorageRawApiPutEntry() {}
  CacheStorageRawApiPutEntry::~CacheStorageRawApiPutEntry() {
    if (entry_)
      entry_->Close();
    entry_ = nullptr;
  }

  void CacheStorageRawApiPutEntry::Run(
    const base::FilePath& path,
    disk_cache::Backend* backend,
    std::unique_ptr<RawEntry> cacheEntry,
    PutEntryResultCallback callback
  ) {
    if (in_progress_) {
      // put task in the queue and quit
      queue_.push_back({path, backend, std::move(cacheEntry), std::move(callback)});
      return;
    }
    in_progress_ = true;
    RunHelper(path, backend, std::move(cacheEntry), std::move(callback));
  }

  void CacheStorageRawApiPutEntry::RunHelper(
    const base::FilePath& path,
    disk_cache::Backend* backend,
    std::unique_ptr<RawEntry> cacheEntry,
    PutEntryResultCallback callback
  ) {
    put_entry_callback_ = std::move(callback);
    raw_entry_ = std::move(cacheEntry);

    backend_callback_ = base::BindOnce(
      &CacheStorageRawApiPutEntry::BackendCallback,
      weak_factory_.GetWeakPtr()
    );

    CreateBackendAndRun(path, backend);
  }

  void CacheStorageRawApiPutEntry::SendResponse (std::string status) {
    std::move(put_entry_callback_).Run(status);

    // CheckQueue();
    auto next_callback = base::BindOnce(&CacheStorageRawApiPutEntry::CheckQueue, weak_factory_.GetWeakPtr());

    if (query_cache_recursive_depth_ <= kMaxQueryCacheRecursiveDepth) {
      query_cache_recursive_depth_ += 1;
      std::move(next_callback).Run();
      return;
    }

    query_cache_recursive_depth_ = 0;
    auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();
    task_runner->PostTask(
      FROM_HERE,
      std::move(next_callback));
  }

  void CacheStorageRawApiPutEntry::OnEntryCreated(
    disk_cache::EntryResult result) {
    if (result.net_error() != net::OK) {
      std::string error_message = "disk_cache entry creation failed";
      SendResponse(error_message);
      return;
    }

    entry_ = result.ReleaseEntry();

    auto split_callback = base::SplitOnceCallback(
        base::BindOnce(&CacheStorageRawApiPutEntry::OnFirstStream,
                       weak_factory_.GetWeakPtr()));

    size_t length = raw_entry_->stream0.size();
    scoped_refptr<net::WrappedIOBuffer> buf = base::MakeRefCounted<net::WrappedIOBuffer>(
        base::span<uint8_t>(raw_entry_->stream0)
    );
    int write_result = entry_->WriteData(0, 0, buf.get(), length, std::move(split_callback.first), true);
    if (write_result != net::ERR_IO_PENDING)
      std::move(split_callback.second).Run(write_result);
  }

  void CacheStorageRawApiPutEntry::OnFirstStream(int status) {
    if (status < 0) {
      std::string error_message = "disk_cache writing stream0 failed with error " + std::to_string(status);
      SendResponse(error_message);
      return;
    }

    if (raw_entry_->ranges.has_value()) {
      current_chunk_num_ = 0;
      total_bytes_ = 0;
      WriteSparseEntry();
      return;
    }

    auto split_callback = base::SplitOnceCallback(
        base::BindOnce(&CacheStorageRawApiPutEntry::OnSecondStream,
                       weak_factory_.GetWeakPtr()));

    size_t length = raw_entry_->stream1.size();
    scoped_refptr<net::WrappedIOBuffer> buf = base::MakeRefCounted<net::WrappedIOBuffer>(
        base::span<uint8_t>(raw_entry_->stream1)
    );
    int write_result = entry_->WriteData(1, 0, buf.get(), length, std::move(split_callback.first), true);
    if (write_result != net::ERR_IO_PENDING)
      std::move(split_callback.second).Run(write_result);
  }

  void CacheStorageRawApiPutEntry::WriteSparseEntry() {
    size_t total_bytes = 0;
    for (const std::pair<int64_t, size_t>& range : raw_entry_->ranges.value()) {
      total_bytes += range.second;
    }
    if (total_bytes > raw_entry_->stream1.size()) {
      std::string error_message = 
        "disk_cache writing sparse stream: stream is illformed, expected " +
        std::to_string(total_bytes) +
        " bytes, received " +
        std::to_string(raw_entry_->stream1.size());
      SendResponse(error_message);
      return;
    }
    WriteNextChunk();
  }

  void CacheStorageRawApiPutEntry::WriteNextChunk() {
    if (current_chunk_num_ == raw_entry_->ranges.value().size()) {
      SendResponse("ok");
      return;
    }

    int64_t offset = raw_entry_->ranges.value()[current_chunk_num_].first;
    size_t length = raw_entry_->ranges.value()[current_chunk_num_].second;

    auto split_callback = base::SplitOnceCallback(
        base::BindOnce(&CacheStorageRawApiPutEntry::OnChunk,
                       weak_factory_.GetWeakPtr()));

    auto buf = base::MakeRefCounted<net::WrappedIOBuffer>(
        UNSAFE_BUFFERS(base::span<uint8_t>(raw_entry_->stream1.data() + total_bytes_, length))
    );
    total_bytes_ += length;
    current_chunk_num_++;

    int write_result = entry_->WriteSparseData(offset, buf.get(), length, std::move(split_callback.first));
    if (write_result != net::ERR_IO_PENDING)
      std::move(split_callback.second).Run(write_result);
  }

  void CacheStorageRawApiPutEntry::OnChunk(int status) {
    if (status < 0) {
      std::string error_message = "disk_cache writing sparse stream failed with error " + std::to_string(status);
      SendResponse(error_message);
      return;
    }

    WriteNextChunk();
  }

  void CacheStorageRawApiPutEntry::OnSecondStream(int status) {
    if (status < 0) {
      std::string error_message = "disk_cache writing stream1 failed with error " + std::to_string(status);
      SendResponse(error_message);
      return;
    }

    size_t length = raw_entry_->stream2.size();
    if (length == 0) {
      SendResponse("ok");
      return;
    }

    auto split_callback = base::SplitOnceCallback(
        base::BindOnce(&CacheStorageRawApiPutEntry::OnThirdStream,
                       weak_factory_.GetWeakPtr()));

    scoped_refptr<net::WrappedIOBuffer> buf = base::MakeRefCounted<net::WrappedIOBuffer>(
        base::span<uint8_t>(raw_entry_->stream2)
    );

    int write_result = entry_->WriteData(2, 0, buf.get(), length, std::move(split_callback.first), true);
    if (write_result != net::ERR_IO_PENDING)
      std::move(split_callback.second).Run(write_result);
  }

  void CacheStorageRawApiPutEntry::OnThirdStream(int status) {
    if (status < 0) {
      std::string error_message = "disk_cache writing stream2 failed with error " + std::to_string(status);
      SendResponse(error_message);
      return;
    }

    SendResponse("ok");
  }

  void CacheStorageRawApiPutEntry::BackendCallback() {
    if (backend_error_ != net::OK) {
      std::string error_message = "disk_cache backend failed with error " + std::to_string(backend_error_);
      SendResponse(error_message);
      return;
    }

    auto split_callback = base::SplitOnceCallback(
        base::BindOnce(&CacheStorageRawApiPutEntry::OnEntryCreated,
                       weak_factory_.GetWeakPtr()));
    disk_cache::EntryResult entry_result = backend_->OpenOrCreateEntry(
        raw_entry_->key, net::DEFAULT_PRIORITY,
        std::move(split_callback.first)
    );
    if (entry_result.net_error() == net::ERR_IO_PENDING)
      return;

    std::move(split_callback.second).Run(std::move(entry_result));
  }

void CacheStorageRawApiPutEntry::CheckQueue () {
    if (entry_) {
      entry_->Close();
      entry_ = nullptr;
    }
    raw_entry_ = nullptr;

    current_chunk_num_ = 0;
    total_bytes_ = 0;

    if (!manage_backend_) {
      backend_.release();
    }
    backend_ = nullptr;

    if (queue_.size() == 0) {
      query_cache_recursive_depth_ = 0;
      in_progress_ = false;
      return;
    }

    auto& [path_, backend_, cacheEntry_, callback_] = queue_.front();
    base::FilePath path = std::move(path_);
    disk_cache::Backend* backend = std::move(backend_);
    std::unique_ptr<RawEntry> cacheEntry = std::move(cacheEntry_);
    PutEntryResultCallback callback = std::move(callback_);
    queue_.pop_front();
    RunHelper(path, backend, std::move(cacheEntry), std::move(callback));
  }
}
