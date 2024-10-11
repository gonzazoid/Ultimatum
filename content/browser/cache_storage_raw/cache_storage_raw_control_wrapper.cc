// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage_raw/cache_storage_raw_control_wrapper.h"
#include "base/task/sequenced_task_runner.h"

namespace content {

CacheStorageRawControlWrapper::CacheStorageRawControlWrapper(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cache_storage_raw_context_ = base::SequenceBound<CacheStorageRawContextImpl>(
      CacheStorageRawContextImpl::CreateSchedulerTaskRunner()
  );
  cache_storage_raw_context_.AsyncCall(&CacheStorageRawContextImpl::Init)
      .WithArgs(cache_storage_raw_control_.BindNewPipeAndPassReceiver());
}

CacheStorageRawControlWrapper::~CacheStorageRawControlWrapper() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void CacheStorageRawControlWrapper::AddReceiver(
    mojo::PendingReceiver<blink::mojom::CacheStorageRaw> receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  cache_storage_raw_control_->AddReceiver(std::move(receiver));
}

}  // namespace content
