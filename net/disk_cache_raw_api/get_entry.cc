// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/io_buffer.h"
#include "net/disk_cache_raw_api/get_entry.h"

namespace disk_cache {

  RawEntryResult::RawEntryResult() = default;
  RawEntryResult::~RawEntryResult() = default;
  RawEntryResult::RawEntryResult(RawEntryResult&& other) = default;
  RawEntryResult& RawEntryResult::operator=(RawEntryResult&& other) = default;

  CacheStorageRawApiGetEntry::CacheStorageRawApiGetEntry() {}
  CacheStorageRawApiGetEntry::~CacheStorageRawApiGetEntry() {}

  void CacheStorageRawApiGetEntry::Run(
    const base::FilePath& path,
    disk_cache::Backend* backend,
    const std::string& key,
    RawEntryResultCallback callback
  ) {
    entry_callback_ = std::move(callback);
    entry_response_ = std::make_unique<RawEntry>();
    entry_response_->key = std::move(key);

    backend_callback_ = base::BindOnce(&CacheStorageRawApiGetEntry::BackendCallback,
                     weak_factory_.GetWeakPtr());
    CreateBackendAndRun(path, backend);
  }

  void CacheStorageRawApiGetEntry::BackendCallback() {
    if (backend_error_ != net::OK) {
      std::string error_message = "disk_cache backend failed with error " + std::to_string(backend_error_);
      SendResponse(error_message);
      return;
    }

    auto split_callback = base::SplitOnceCallback(
      base::BindOnce(&CacheStorageRawApiGetEntry::OnEntryOpened, weak_factory_.GetWeakPtr())
    );
    disk_cache::EntryResult entry_result = backend_->OpenEntry(
        entry_response_->key, net::DEFAULT_PRIORITY,
        std::move(split_callback.first)
    );
    if (entry_result.net_error() == net::ERR_IO_PENDING) {
      return;
    }

    std::move(split_callback.second).Run(std::move(entry_result));
  }

  void CacheStorageRawApiGetEntry::OnEntryOpened(
    disk_cache::EntryResult result) {
    if (result.net_error() == net::ERR_FAILED) {

      // entry not found
      std::string error_message = "not found";
      SendResponse(error_message);
      return;
    }

    entry_ = result.ReleaseEntry();

    size_t length = entry_->GetDataSize(0);
    entry_response_->stream0.resize(length);
    scoped_refptr<net::WrappedIOBuffer> buf = base::MakeRefCounted<net::WrappedIOBuffer>(
        base::span<uint8_t>(entry_response_->stream0)
    );
    auto split_callback = base::SplitOnceCallback(
      base::BindOnce(&CacheStorageRawApiGetEntry::GetFirstStreamCompleted, weak_factory_.GetWeakPtr())
    );

    int status = entry_->ReadData(0, 0, buf.get(), length, std::move(split_callback.first));
    if (status != net::ERR_IO_PENDING) {
      std::move(split_callback.second).Run(status);
    }
  }

  void CacheStorageRawApiGetEntry::GetFirstStreamCompleted(int status) {
    if (status < 0) {
      std::string error_message = "disk_cache reading stream0 failed with error " + std::to_string(status);
      SendResponse(error_message);
      return;
    }

    auto split_callback = base::SplitOnceCallback(
      base::BindOnce(&CacheStorageRawApiGetEntry::GetAvailableRangesCompleted, weak_factory_.GetWeakPtr())
    );
    auto range_result = entry_->GetAvailableRanges(std::move(split_callback.first));
    if (range_result.net_error == net::ERR_IO_PENDING)
      return;
    std::move(split_callback.second).Run(range_result);

  }

  void CacheStorageRawApiGetEntry::GetAvailableRangesCompleted(
    const disk_cache::RangesResult& result) {
    if (result.net_error != net::OK) {
      SendResponse("reading ranges failed with error " + std::to_string(result.net_error));
      return;
    }

    if (result.ranges == nullptr || result.ranges->size() == 0) {
      size_t length = entry_->GetDataSize(1);
      entry_response_->stream1.resize(length);
      scoped_refptr<net::WrappedIOBuffer> buf = base::MakeRefCounted<net::WrappedIOBuffer>(
        base::span<uint8_t>(entry_response_->stream1)
      );
      auto split_callback = base::SplitOnceCallback(
        base::BindOnce(&CacheStorageRawApiGetEntry::GetSecondStreamCompleted, weak_factory_.GetWeakPtr())
      );

      int read_status = entry_->ReadData(1, 0, buf.get(), length, std::move(split_callback.first));
      if (read_status != net::ERR_IO_PENDING) {
        std::move(split_callback.second).Run(read_status);
      }

      return;
    }

    int32_t total_length = 0;
    for (auto it = result.ranges->begin(); it != result.ranges->end(); ++it) {
      chunks_.push_back(std::pair<int64_t, int>(it->start, it->available_len));
      total_length += it->available_len;
    }
    entry_response_->stream1.resize(total_length);
    current_chunk_num_ = 0;
    total_bytes_ = 0;
    ReadNextChunk();
  }

  void CacheStorageRawApiGetEntry::ReadNextChunk() {
    if (current_chunk_num_ == chunks_.size()) {
      // create ranges
      entry_response_->ranges = std::move(chunks_);
      SendResponse("ok");
      return;
    }

    auto split_callback = base::SplitOnceCallback(
      base::BindOnce(&CacheStorageRawApiGetEntry::OnChunk, weak_factory_.GetWeakPtr())
    );

    int64_t offset = chunks_[current_chunk_num_].first;
    size_t length = chunks_[current_chunk_num_].second;

    auto current_chunk = base::MakeRefCounted<net::WrappedIOBuffer>(
        UNSAFE_BUFFERS(base::span<uint8_t>(entry_response_->stream1.data() + total_bytes_, length))
    );

    int read_status = entry_->ReadSparseData(offset, current_chunk.get(), length, std::move(split_callback.first));
    if (read_status != net::ERR_IO_PENDING) {
      std::move(split_callback.second).Run(read_status);
    }
  }

  void CacheStorageRawApiGetEntry::OnChunk(int status) {
    size_t length = chunks_[current_chunk_num_].second;
    current_chunk_num_++;
    total_bytes_ += length;

    ReadNextChunk();
  }

  void CacheStorageRawApiGetEntry::GetSecondStreamCompleted(int status) {
    if (status < 0) {
      std::string error_message = "disk_cache reading stream1 failed with error " + std::to_string(status);
      SendResponse(error_message);
      return;
    }

    size_t length = entry_->GetDataSize(2);
    if (length == 0) {
      SendResponse("ok");
      return;
    }

    entry_response_->stream2.resize(length);
    scoped_refptr<net::WrappedIOBuffer> buf = base::MakeRefCounted<net::WrappedIOBuffer>(
      base::span<uint8_t>(entry_response_->stream2)
    );
    auto split_callback = base::SplitOnceCallback(
      base::BindOnce(&CacheStorageRawApiGetEntry::GetThirdStreamCompleted, weak_factory_.GetWeakPtr())
    );

    int third_stream_status = entry_->ReadData(2, 0, buf.get(), length, std::move(split_callback.first));
    if (third_stream_status != net::ERR_IO_PENDING) {
      std::move(split_callback.second).Run(third_stream_status);
    }
  }

  void CacheStorageRawApiGetEntry::GetThirdStreamCompleted(int status) {
    if (status < 0) {
      std::string error_message = "disk_cache reading stream2 failed with error " + std::to_string(status);
      SendResponse(error_message);
      return;
    }

    SendResponse("ok");
  }

  void CacheStorageRawApiGetEntry::SendResponse (std::string status) {
    entry_->Close();
    entry_ = nullptr;
    auto response = std::make_unique<RawEntryResult>();
    response->status = status;
    if (status == "ok")
      response->entry = std::move(entry_response_);
    std::move(entry_callback_).Run(std::move(response));
  }
}
