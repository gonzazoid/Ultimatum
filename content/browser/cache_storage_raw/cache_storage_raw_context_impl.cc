// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage_raw/cache_storage_raw_context_impl.h"

#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "content/browser/cache_storage_raw/cache_storage_raw_dispatcher_host.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace content {

CacheStorageRawContextImpl::CacheStorageRawContextImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

CacheStorageRawContextImpl::~CacheStorageRawContextImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
scoped_refptr<base::SequencedTaskRunner>
CacheStorageRawContextImpl::CreateSchedulerTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::USER_VISIBLE});
}

void CacheStorageRawContextImpl::Init(
    mojo::PendingReceiver<storage::mojom::CacheStorageRawControl> control) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  receivers_.Add(this, std::move(control));

  scoped_refptr<base::SequencedTaskRunner> cache_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  DCHECK(!dispatcher_host_);
  dispatcher_host_ =
      std::make_unique<CacheStorageRawDispatcherHost>(this);

}

void CacheStorageRawContextImpl::AddReceiver(
    mojo::PendingReceiver<blink::mojom::CacheStorageRaw> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  dispatcher_host_->AddReceiver(std::move(receiver));

}

}  // namespace content
