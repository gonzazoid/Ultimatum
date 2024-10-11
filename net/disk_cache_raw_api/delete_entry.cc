// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache_raw_api/delete_entry.h"

namespace disk_cache {

  CacheStorageRawApiDeleteEntry::CacheStorageRawApiDeleteEntry() {}
  CacheStorageRawApiDeleteEntry::~CacheStorageRawApiDeleteEntry() {}

  void CacheStorageRawApiDeleteEntry::Run(
    const base::FilePath& path,
    disk_cache::Backend* backend,
    const std::string& key,
    DeleteEntryResultCallback callback
  ) {
    if (in_progress_) {
      // put task in the queue and quit
      queue_.push_back({path, backend, key, std::move(callback)});
      return;
    }
    in_progress_ = true;
    RunHelper(path, backend, key, std::move(callback));
  }

  void CacheStorageRawApiDeleteEntry::RunHelper(
    const base::FilePath& path,
    disk_cache::Backend* backend,
    const std::string& key,
    DeleteEntryResultCallback callback
  ) {
    delete_callback_ = std::move(callback);

    backend_callback_ = base::BindOnce(
      &CacheStorageRawApiDeleteEntry::BackendCallback,
      weak_factory_.GetWeakPtr(),
      key
    );
    CreateBackendAndRun(path, backend);
  }

  void CacheStorageRawApiDeleteEntry::SendResponse (std::string status) {
    std::move(delete_callback_).Run(status);

    // CheckQueue();
    auto next_callback = base::BindOnce(&CacheStorageRawApiDeleteEntry::CheckQueue, weak_factory_.GetWeakPtr());

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

  void CacheStorageRawApiDeleteEntry::CheckQueue () {
    if (!manage_backend_) {
      backend_.release();
    }
    backend_ = nullptr;

    if (queue_.size() == 0) {
      in_progress_ = false;
      query_cache_recursive_depth_ = 0;
      return;
    }

    auto& [path_, backend_, key_, callback_] = queue_.front();
    base::FilePath path = std::move(path_);
    disk_cache::Backend* backend = std::move(backend_);
    const std::string key = std::move(key_);
    DeleteEntryResultCallback callback = std::move(callback_);
    queue_.pop_front();
    RunHelper(path, backend, key, std::move(callback));
  }

  void CacheStorageRawApiDeleteEntry::OnEntryDoomed(
    int result) {
    if (result == net::OK) {
      SendResponse("ok");
    } else {
      std::string error_message = "disk_cache DoomEntry failed with error " + std::to_string(result);
      SendResponse(error_message);
    }
  }

  void CacheStorageRawApiDeleteEntry::BackendCallback(
      const std::string& key
  ) {
    if (backend_error_ != net::OK) {
      std::string error_message = "disk_cache backend failed with error " + std::to_string(backend_error_);
      SendResponse(error_message);
      return;
    }

    auto split_callback = base::SplitOnceCallback(
      base::BindOnce(&CacheStorageRawApiDeleteEntry::OnEntryDoomed, weak_factory_.GetWeakPtr())
    );
    int entry_result = backend_->DoomEntry(
        key, net::DEFAULT_PRIORITY,
        std::move(split_callback.first)
    );
    if (entry_result == net::ERR_IO_PENDING) {
      return;
    }

    std::move(split_callback.second).Run(std::move(entry_result));
  }
}
