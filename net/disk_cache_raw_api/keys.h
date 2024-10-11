// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_DISK_CACHE_RAW_API_KEYS_H_
#define NET_DISK_CACHE_DISK_CACHE_RAW_API_KEYS_H_

#include <tuple>
#include <deque>

#include "net/disk_cache_raw_api/api.h"

namespace disk_cache {
class NET_EXPORT KeysResult {
 public:
  KeysResult();
  ~KeysResult();
  KeysResult(KeysResult&&);
  KeysResult& operator=(KeysResult&&);

  KeysResult(const KeysResult&) = delete;
  KeysResult& operator=(const KeysResult&) = delete;

  std::string status;
  std::optional<std::vector<std::string>> keys;
};

using KeysResultCallback = base::OnceCallback<void(std::unique_ptr<KeysResult>)>;
using KeysTask = std::tuple<base::FilePath, disk_cache::Backend*, KeysResultCallback>;

class NET_EXPORT CacheStorageRawApiKeys: public virtual CacheStorageRawApi {
 public:

  CacheStorageRawApiKeys();
  CacheStorageRawApiKeys(const CacheStorageRawApiKeys&) = delete;
  CacheStorageRawApiKeys& operator=(const CacheStorageRawApiKeys&) = delete;

  ~CacheStorageRawApiKeys();

  void Run(
    const base::FilePath& path,
    disk_cache::Backend* backend,
    KeysResultCallback callback
  );

 private:

  void RunHelper(
    const base::FilePath& path,
    disk_cache::Backend* backend,
    KeysResultCallback callback
  );

  void BackendCallback();
  void StartIteration();
  void Iterate();
  void HandleCacheEntry(disk_cache::EntryResult result);
  void SendErrKeysResponse(std::string error);
  void CheckQueue();

  std::unique_ptr<disk_cache::Backend::Iterator> iterator_;
  std::vector<std::string> keys_;

  KeysResultCallback keys_callback_;
  bool in_progress_ = false;
  std::deque<KeysTask> queue_;

  base::WeakPtrFactory<CacheStorageRawApiKeys> weak_factory_{this};
};
}
#endif  // NET_DISK_CACHE_DISK_CACHE_RAW_API_KEYS_H_
