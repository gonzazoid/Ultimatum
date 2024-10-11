// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_DISK_CACHE_RAW_API_GET_ENTRY_H_
#define NET_DISK_CACHE_DISK_CACHE_RAW_API_GET_ENTRY_H_

#include "net/disk_cache_raw_api/api.h"

namespace disk_cache {

class NET_EXPORT RawEntryResult {
 public:
  RawEntryResult();
  ~RawEntryResult();
  RawEntryResult(RawEntryResult&&);
  RawEntryResult& operator=(RawEntryResult&&);

  RawEntryResult(const RawEntryResult&) = delete;
  RawEntryResult& operator=(const RawEntryResult&) = delete;

  std::string status;
  std::unique_ptr<RawEntry> entry;
};

using RawEntryResultCallback = base::OnceCallback<void(std::unique_ptr<RawEntryResult>)>;

class NET_EXPORT CacheStorageRawApiGetEntry: public virtual CacheStorageRawApi {
 public:

  CacheStorageRawApiGetEntry();
  CacheStorageRawApiGetEntry(const CacheStorageRawApiGetEntry&) = delete;
  CacheStorageRawApiGetEntry& operator=(const CacheStorageRawApiGetEntry&) = delete;

  ~CacheStorageRawApiGetEntry();

  void Run(
    const base::FilePath& path,
    disk_cache::Backend* backend,
    const std::string& key,
    RawEntryResultCallback callback
  );

 private:

  void BackendCallback();
  void OnEntryOpened(disk_cache::EntryResult result);
  void GetFirstStreamCompleted(int status);
  void GetAvailableRangesCompleted(const disk_cache::RangesResult& result);
  void GetSecondStreamCompleted(int status);
  void GetThirdStreamCompleted(int status);
  void SendResponse(std::string error);
  void ReadNextChunk();
  void OnChunk(int status);

  raw_ptr<disk_cache::Entry> entry_;
  std::unique_ptr<RawEntry> entry_response_;

  std::vector<std::pair<int64_t, size_t>> chunks_;
  size_t current_chunk_num_;
  size_t total_bytes_;
  RawEntryResultCallback entry_callback_;

  base::WeakPtrFactory<CacheStorageRawApiGetEntry> weak_factory_{this};
};
}
#endif  // NET_DISK_CACHE_DISK_CACHE_RAW_API_GET_ENTRY_H_
