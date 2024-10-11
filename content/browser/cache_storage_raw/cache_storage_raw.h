// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_RAW_CACHE_STORAGE_RAW_H_
#define CONTENT_BROWSER_CACHE_STORAGE_RAW_CACHE_STORAGE_RAW_H_

#include "mojo/public/cpp/bindings/receiver.h"
#include "content/common/content_export.h"
#include "services/network/public/mojom/disk_cache_raw_api.mojom.h"
#include "third_party/blink/public/mojom/cache_storage_raw/cache_storage_raw.mojom.h"

#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache_raw_api/keys.h"
#include "net/disk_cache_raw_api/get_entry.h"
#include "net/disk_cache_raw_api/put_entry.h"
#include "net/disk_cache_raw_api/delete_entry.h"
#include "net/base/io_buffer.h"

namespace content {

class CacheStorageRaw: public blink::mojom::CacheStorageRaw {
 public:

  CacheStorageRaw();
  CacheStorageRaw(const CacheStorageRaw&) = delete;
  CacheStorageRaw& operator=(const CacheStorageRaw&) = delete;

  ~CacheStorageRaw() override;

  // Mojo CacheStorageRaw Interface implementation:
  void Keys(
    const base::FilePath& path,
    blink::mojom::CacheStorageRaw::KeysCallback callback
  ) override;

  void GetCacheEntry(
    const base::FilePath& path,
    const ::std::string& key,
    blink::mojom::CacheStorageRaw::GetCacheEntryCallback callback
  ) override;

  void PutCacheEntry(
    const base::FilePath& path,
    network::mojom::DiskCacheEntryPtr cacheEntry,
    blink::mojom::CacheStorageRaw::PutCacheEntryCallback callback
  ) override;

  void DeleteCacheEntry(
    const base::FilePath& path,
    const ::std::string& key,
    blink::mojom::CacheStorageRaw::DeleteCacheEntryCallback callback
  ) override;

 private:
  void OnDiskCacheKeys(
    blink::mojom::CacheStorageRaw::KeysCallback callback,
    std::unique_ptr<disk_cache::KeysResult> keysResult
  );

  void OnDiskCacheRawEntry(
    blink::mojom::CacheStorageRaw::GetCacheEntryCallback callback,
    std::unique_ptr<disk_cache::RawEntryResult> entryResult
  );

  void OnDiskCachePutEntry(
    blink::mojom::CacheStorageRaw::PutCacheEntryCallback callback,
    std::string& putEntryResult
  );

  void OnDiskCacheDeleteEntry(
    blink::mojom::CacheStorageRaw::DeleteCacheEntryCallback callback,
    std::string& deleteEntryResult
  );

  std::unique_ptr<disk_cache::CacheStorageRawApiKeys> keys_exec_;
  std::unique_ptr<disk_cache::CacheStorageRawApiGetEntry> get_entry_exec_;
  std::unique_ptr<disk_cache::CacheStorageRawApiPutEntry> put_entry_exec_;
  std::unique_ptr<disk_cache::CacheStorageRawApiDeleteEntry> delete_entry_exec_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<CacheStorageRaw> weak_factory_{this};
};
}
#endif  // CONTENT_BROWSER_CACHE_STORAGE_RAW_CACHE_STORAGE_RAW_H_
