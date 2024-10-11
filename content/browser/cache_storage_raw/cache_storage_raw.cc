// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage_raw/cache_storage_raw.h"

namespace content {

  CacheStorageRaw::CacheStorageRaw() {}

  CacheStorageRaw::~CacheStorageRaw() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  // Mojo CacheStorage Interface implementation:
  void CacheStorageRaw::OnDiskCacheKeys(blink::mojom::CacheStorageRaw::KeysCallback callback, std::unique_ptr<disk_cache::KeysResult> keysResult) {
    network::mojom::DiskCacheKeysResponsePtr response = network::mojom::DiskCacheKeysResponse::New();
    response->status = std::move(keysResult->status);
    if (keysResult->status == "ok") {
      response->keys = keysResult->keys;
    }
    std::move(callback).Run(std::move(response));
  }

  void CacheStorageRaw::Keys(const base::FilePath& path, blink::mojom::CacheStorageRaw::KeysCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto keys_callback = base::BindOnce(&CacheStorageRaw::OnDiskCacheKeys,
                     weak_factory_.GetWeakPtr(), std::move(callback));
    if (!keys_exec_)
      keys_exec_ = std::make_unique<disk_cache::CacheStorageRawApiKeys>();
    keys_exec_->Run(path, nullptr, std::move(keys_callback));
  }

  void CacheStorageRaw::OnDiskCacheRawEntry(blink::mojom::CacheStorageRaw::GetCacheEntryCallback callback, std::unique_ptr<disk_cache::RawEntryResult> entryResult) {
    network::mojom::DiskCacheEntryResponsePtr response = network::mojom::DiskCacheEntryResponse::New();
    response->status = std::move(entryResult->status);
    if (entryResult->status == "ok") {
      network::mojom::DiskCacheEntryPtr entry = network::mojom::DiskCacheEntry::New();
      entry->key = std::move(entryResult->entry->key);
      entry->stream0 = std::move(entryResult->entry->stream0);
      entry->stream1 = std::move(entryResult->entry->stream1);
      entry->stream2 = std::move(entryResult->entry->stream2);
      if (entryResult->entry->ranges.has_value()) {
        std::vector<network::mojom::DiskCacheEntryRangePtr> new_ranges = {};
        for (const std::pair<int64_t, size_t> range : entryResult->entry->ranges.value()) {
          network::mojom::DiskCacheEntryRangePtr new_range = network::mojom::DiskCacheEntryRange::New(range.first, range.second);
          new_ranges.push_back(std::move(new_range));
        }
        entry->ranges = std::move(new_ranges);
      }
      response->entry = std::move(entry);
    }
    std::move(callback).Run(std::move(response));
  }

  void CacheStorageRaw::GetCacheEntry(const base::FilePath& path, const ::std::string& key, blink::mojom::CacheStorageRaw::GetCacheEntryCallback callback) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    auto entry_callback = base::BindOnce(&CacheStorageRaw::OnDiskCacheRawEntry,
                     weak_factory_.GetWeakPtr(), std::move(callback));

    if (!get_entry_exec_)
      get_entry_exec_ = std::make_unique<disk_cache::CacheStorageRawApiGetEntry>();
    get_entry_exec_->Run(path, nullptr, key, std::move(entry_callback));
  }

  void CacheStorageRaw::OnDiskCacheDeleteEntry(
    blink::mojom::CacheStorageRaw::DeleteCacheEntryCallback callback,
    std::string& deleteEntryResult) {
    std::move(callback).Run(deleteEntryResult);
  }

  void CacheStorageRaw::DeleteCacheEntry(
    const base::FilePath& path,
    const ::std::string& key,
    blink::mojom::CacheStorageRaw::DeleteCacheEntryCallback callback
  ) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    auto delete_callback = base::BindOnce(&CacheStorageRaw::OnDiskCacheDeleteEntry,
                     weak_factory_.GetWeakPtr(), std::move(callback));

    if (!delete_entry_exec_)
      delete_entry_exec_ = std::make_unique<disk_cache::CacheStorageRawApiDeleteEntry>();
    delete_entry_exec_->Run(path, nullptr, key, std::move(delete_callback));
  }

  void CacheStorageRaw::OnDiskCachePutEntry(
    blink::mojom::CacheStorageRaw::PutCacheEntryCallback callback,
    std::string& putEntryResult) {
    std::move(callback).Run(putEntryResult);
  }

  void CacheStorageRaw::PutCacheEntry(
    const base::FilePath& path,
    network::mojom::DiskCacheEntryPtr cacheEntry,
    blink::mojom::CacheStorageRaw::PutCacheEntryCallback callback
  ) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    auto entry = std::make_unique<disk_cache::RawEntry>();
    entry->key = std::move(cacheEntry->key);
    entry->stream0 = std::move(cacheEntry->stream0);
    entry->stream1 = std::move(cacheEntry->stream1);
    entry->stream2 = std::move(cacheEntry->stream2);
    if (cacheEntry->ranges.has_value()) {
      std::vector<std::pair<int64_t, size_t>> new_ranges = {};
      for (const network::mojom::DiskCacheEntryRangePtr& range : cacheEntry->ranges.value()) {
        auto new_range = std::make_pair(range->offset, static_cast<size_t>(range->length));
        new_ranges.push_back(std::move(new_range));
      }
      entry->ranges = std::move(new_ranges);
    }

    auto put_callback = base::BindOnce(&CacheStorageRaw::OnDiskCachePutEntry,
                     weak_factory_.GetWeakPtr(), std::move(callback));

    if (!put_entry_exec_)
      put_entry_exec_ = std::make_unique<disk_cache::CacheStorageRawApiPutEntry>();
    put_entry_exec_->Run(path, nullptr, std::move(entry), std::move(put_callback));
  }
}
