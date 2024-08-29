// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_DISK_CACHE_RAW_API_PUT_ENTRY_H_
#define NET_DISK_CACHE_DISK_CACHE_RAW_API_PUT_ENTRY_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/cache_storage_raw/cache_storage_raw.mojom.h"

#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache_raw_api/api.h"

namespace disk_cache {

using PutEntryResultCallback = base::OnceCallback<void(std::string&)>;

class NET_EXPORT CacheStorageRawApiPutEntry: public virtual CacheStorageRawApi {
 public:

  CacheStorageRawApiPutEntry();
  CacheStorageRawApiPutEntry(const CacheStorageRawApiPutEntry&) = delete;
  CacheStorageRawApiPutEntry& operator=(const CacheStorageRawApiPutEntry&) = delete;

  ~CacheStorageRawApiPutEntry();

  void Run(
    const base::FilePath& path,
    disk_cache::Backend* backend,
    std::unique_ptr<RawEntry> cacheEntry,
    PutEntryResultCallback callback
  );

 private:

  void BackendCallback();
  void OnEntryCreated(disk_cache::EntryResult result);

  void SendResponse (std::string error);

  void OnFirstStream(int status);
  void OnSecondStream(int status);
  void OnThirdStream(int status);

  void WriteSparseEntry();
  void WriteNextChunk();
  void OnChunk(int status);

  raw_ptr<disk_cache::Entry> entry_;
  std::unique_ptr<RawEntry> raw_entry_;
  PutEntryResultCallback put_entry_callback_;

  size_t current_chunk_num_;
  size_t total_bytes_;

  base::WeakPtrFactory<CacheStorageRawApiPutEntry> weak_factory_{this};
};
}
#endif  // NET_DISK_CACHE_DISK_CACHE_RAW_API_PUT_ENTRY_H_
