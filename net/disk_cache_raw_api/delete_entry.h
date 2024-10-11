// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_DISK_CACHE_RAW_API_DELETE_ENTRY_H_
#define NET_DISK_CACHE_DISK_CACHE_RAW_API_DELETE_ENTRY_H_

#include <tuple>
#include <deque>

#include "net/disk_cache_raw_api/api.h"

namespace disk_cache {

using DeleteEntryResultCallback = base::OnceCallback<void(std::string&)>;
using DeleteEntryTask = std::tuple<base::FilePath, disk_cache::Backend*, std::string, DeleteEntryResultCallback>;

class NET_EXPORT CacheStorageRawApiDeleteEntry: public virtual CacheStorageRawApi {
 public:

  CacheStorageRawApiDeleteEntry();
  CacheStorageRawApiDeleteEntry(const CacheStorageRawApiDeleteEntry&) = delete;
  CacheStorageRawApiDeleteEntry& operator=(const CacheStorageRawApiDeleteEntry&) = delete;

  ~CacheStorageRawApiDeleteEntry();

  void Run(
    const base::FilePath& path,
    disk_cache::Backend* backend,
    const std::string& key,
    DeleteEntryResultCallback callback
  );

 private:

  void RunHelper(
    const base::FilePath& path,
    disk_cache::Backend* backend,
    const std::string& key,
    DeleteEntryResultCallback callback
  );

  void BackendCallback(const std::string& key);
  void SendResponse (std::string status);
  void OnEntryDoomed(int result);
  void CheckQueue();

  bool in_progress_ = false;
  std::deque<DeleteEntryTask> queue_;
  DeleteEntryResultCallback delete_callback_;

  base::WeakPtrFactory<CacheStorageRawApiDeleteEntry> weak_factory_{this};
};
}
#endif  // NET_DISK_CACHE_DISK_CACHE_RAW_API_DELETE_ENTRY_H_
