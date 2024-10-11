// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_RAW_CACHE_STORAGE_RAW_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_CACHE_STORAGE_RAW_CACHE_STORAGE_RAW_DISPATCHER_HOST_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "components/services/storage/public/mojom/cache_storage_raw_control.mojom.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/unique_associated_receiver_set.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "third_party/blink/public/mojom/cache_storage_raw/cache_storage_raw.mojom.h"

namespace content {

class CacheStorageRawContextImpl;

class CacheStorageRawDispatcherHost {
 public:
  CacheStorageRawDispatcherHost(CacheStorageRawContextImpl* context);

  CacheStorageRawDispatcherHost(const CacheStorageRawDispatcherHost&) = delete;
  CacheStorageRawDispatcherHost& operator=(const CacheStorageRawDispatcherHost&) =
      delete;

  ~CacheStorageRawDispatcherHost();

  raw_ptr<CacheStorageRawContextImpl> context() { return context_; }
  // Binds the CacheStorageRaw Mojo receiver to this instance.
  void AddReceiver(
      mojo::PendingReceiver<blink::mojom::CacheStorageRaw> receiver);

  base::WeakPtr<CacheStorageRawDispatcherHost> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  class CacheStorageRawImpl;
  // `this` is owned by `context_`.
  const raw_ptr<CacheStorageRawContextImpl> context_;

  mojo::UniqueReceiverSet<blink::mojom::CacheStorageRaw> receivers_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<CacheStorageRawDispatcherHost> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_RAW_CACHE_STORAGE_RAW_DISPATCHER_HOST_H_
