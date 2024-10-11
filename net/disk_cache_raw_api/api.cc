// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"
#include "net/disk_cache_raw_api/api.h"

namespace disk_cache {

  RawEntry::RawEntry() = default;
  RawEntry::~RawEntry() = default;

  RawEntry::RawEntry(RawEntry&& other) = default;
  RawEntry& RawEntry::operator=(RawEntry&& other) = default;

  CacheStorageRawApi::CacheStorageRawApi() {}
  CacheStorageRawApi::~CacheStorageRawApi() {
    if (!manage_backend_)
      backend_.release();
  }

  void CacheStorageRawApi::CreateBackendAndRun(const base::FilePath& path, Backend* backend) {
    DCHECK(!backend_);
    if (backend) {
      backend_error_ = net::OK;
      if (!manage_backend_) {
        backend_.release();
      }
      manage_backend_ = false;
      backend_.reset(backend);
      std::move(backend_callback_).Run();
      return;
    }

    net::CacheType cache_type = net::DISK_CACHE;

    // The maximum size of each cache. Ultimately, cache size
    // is controlled per storage key by the QuotaManager.
    int64_t max_bytes = std::numeric_limits<int64_t>::max();

    auto split_callback = base::SplitOnceCallback(
      base::BindOnce(
        &CacheStorageRawApi::OnBackendDidCreate,
        weak_factory_.GetWeakPtr()
      )
    );

    disk_cache::BackendResult result = disk_cache::CreateCacheBackend(
        cache_type, net::CACHE_BACKEND_SIMPLE, /*file_operations=*/nullptr, path,
        max_bytes, disk_cache::ResetHandling::kNeverReset, /*net_log=*/nullptr, /*cache_encryption_delegate=*/nullptr,
        /* base::BindOnce(&CacheStorageRawApi::DeleteBackendCompletedIO,
                       weak_factory_.GetWeakPtr()), */ /* post_cleanup_callback */
        std::move(split_callback.first));
    if (result.net_error != net::ERR_IO_PENDING)
      std::move(split_callback.second).Run(std::move(result));
  }

  void CacheStorageRawApi::OnBackendDidCreate(
    disk_cache::BackendResult result
  ) {
    backend_error_ = result.net_error;
    if (result.net_error == net::OK) {
      backend_ = std::move(result.backend);
    }

    std::move(backend_callback_).Run();
  }

  void CacheStorageRawApi::DeleteBackendCompletedIO() {}

}
