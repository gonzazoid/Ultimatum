// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_RAW_CACHE_STORAGE_RAW_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_CACHE_STORAGE_RAW_CACHE_STORAGE_RAW_CONTEXT_IMPL_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "components/services/storage/public/mojom/cache_storage_raw_control.mojom.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/cache_storage_raw/cache_storage_raw.mojom-forward.h"

namespace base {
class SequencedTaskRunner;
}

namespace content {

class CacheStorageRawDispatcherHost;
class CacheStorageRawManager;

// This class is an implementation of the CacheStorageRawControl mojom that is
// called from the browser.  One instance of this exists per StoragePartition,
// and services multiple child processes/origins.  (Compare this with
// CacheStorageRawDispatcherHost which handles renderer <-> storage service mojo
// messages.)  All functions must be called on the same sequence that the
// object is constructed on.
class CONTENT_EXPORT CacheStorageRawContextImpl
    : public storage::mojom::CacheStorageRawControl {
 public:
  explicit CacheStorageRawContextImpl();
  ~CacheStorageRawContextImpl() override;

  static scoped_refptr<base::SequencedTaskRunner> CreateSchedulerTaskRunner();

  void Init(mojo::PendingReceiver<storage::mojom::CacheStorageRawControl> control);

  // storage::mojom::CacheStorageControl implementation.
  void AddReceiver(
      mojo::PendingReceiver<blink::mojom::CacheStorageRaw> receiver) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  mojo::ReceiverSet<storage::mojom::CacheStorageRawControl> receivers_;

  std::unique_ptr<CacheStorageRawDispatcherHost> dispatcher_host_;

  base::WeakPtrFactory<CacheStorageRawContextImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_RAW_CACHE_STORAGE_RAW_CONTEXT_IMPL_H_
