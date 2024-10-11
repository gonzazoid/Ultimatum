// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_RAW_CACHE_STORAGE_RAW_CONTROL_WRAPPER_H_
#define CONTENT_BROWSER_CACHE_STORAGE_RAW_CACHE_STORAGE_RAW_CONTROL_WRAPPER_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/services/storage/public/mojom/cache_storage_raw_control.mojom.h"
#include "content/browser/cache_storage_raw/cache_storage_raw_context_impl.h"

namespace content {

class CacheStorageRawControlWrapper : public storage::mojom::CacheStorageRawControl {
 public:
  CacheStorageRawControlWrapper(
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);
  ~CacheStorageRawControlWrapper() override;

  CacheStorageRawControlWrapper(const CacheStorageRawControlWrapper&) = delete;
  CacheStorageRawControlWrapper& operator=(const CacheStorageRawControlWrapper&) =
      delete;

  storage::mojom::CacheStorageRawControl* GetCacheStorageRawControl() {
    return cache_storage_raw_control_.get();
  }

  // storage::mojom::CacheStorageRawControl implementation.
  void AddReceiver(
      mojo::PendingReceiver<blink::mojom::CacheStorageRaw> receiver) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  base::SequenceBound<CacheStorageRawContextImpl> cache_storage_raw_context_;
  mojo::Remote<storage::mojom::CacheStorageRawControl> cache_storage_raw_control_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_RAW_CACHE_STORAGE_RAW_CONTROL_WRAPPER_H_
