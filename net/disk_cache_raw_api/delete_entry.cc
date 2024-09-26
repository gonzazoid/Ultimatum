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
