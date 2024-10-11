// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage_raw/cache_storage_raw_dispatcher_host.h"

#include "content/browser/cache_storage_raw/cache_storage_raw.h"
#include "content/browser/cache_storage_raw/cache_storage_raw_context_impl.h"

namespace content {

CacheStorageRawDispatcherHost::CacheStorageRawDispatcherHost(
    CacheStorageRawContextImpl* context)
    : context_(context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

CacheStorageRawDispatcherHost::~CacheStorageRawDispatcherHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CacheStorageRawDispatcherHost::AddReceiver(
    mojo::PendingReceiver<blink::mojom::CacheStorageRaw> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto impl = std::make_unique<CacheStorageRaw>();
  receivers_.Add(std::move(impl), std::move(receiver));
}

}  // namespace content
