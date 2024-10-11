// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_DISK_CACHE_RAW_API_H_
#define NET_DISK_CACHE_DISK_CACHE_RAW_API_H_

#include "net/disk_cache/disk_cache.h"

namespace disk_cache {
// Maximum number of recursive calls we permit
// before forcing an asynchronous task.
const int kMaxQueryCacheRecursiveDepth = 20;
class NET_EXPORT RawEntry {
 public:
  RawEntry();
  ~RawEntry();
  RawEntry(RawEntry&&);
  RawEntry& operator=(RawEntry&&);

  RawEntry(const RawEntry&) = delete;
  RawEntry& operator=(const RawEntry&) = delete;

  std::string key;
  std::vector<uint8_t> stream0;
  std::vector<uint8_t> stream1;
  std::vector<uint8_t> stream2;
  std::optional<std::vector<std::pair<int64_t, size_t>>> ranges;
};

class CacheStorageRawApi {
 public:

  CacheStorageRawApi();
  CacheStorageRawApi(const CacheStorageRawApi&) = delete;
  CacheStorageRawApi& operator=(const CacheStorageRawApi&) = delete;

  ~CacheStorageRawApi();

 protected:

  void CreateBackendAndRun(const base::FilePath& path, Backend* backend);

  void OnBackendDidCreate(
    disk_cache::BackendResult result
  );

  void DeleteBackendCompletedIO();

  int query_cache_recursive_depth_ = 0;
  std::unique_ptr<Backend> backend_;
  bool manage_backend_ = true;
  net::Error backend_error_;
  base::OnceCallback<void()> backend_callback_;

 private:
  base::WeakPtrFactory<CacheStorageRawApi> weak_factory_{this};
};
}
#endif  // NET_DISK_CACHE_DISK_CACHE_RAW_API_H_
