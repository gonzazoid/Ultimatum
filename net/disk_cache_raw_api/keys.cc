// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache_raw_api/keys.h"

namespace disk_cache {
  // Maximum number of recursive OpenNextEntry() calls we permit
  // before forcing an asynchronous task.
  const int kMaxQueryCacheRecursiveDepth = 20;

  KeysResult::KeysResult() = default;
  KeysResult::~KeysResult() = default;

  KeysResult::KeysResult(KeysResult&& other) = default;
  KeysResult& KeysResult::operator=(KeysResult&& other) = default;

  CacheStorageRawApiKeys::CacheStorageRawApiKeys() {}
  CacheStorageRawApiKeys::~CacheStorageRawApiKeys() {}

  void CacheStorageRawApiKeys::Run(
    const base::FilePath& path,
    Backend* backend,
    KeysResultCallback callback
  ) {
    keys_callback_ = std::move(callback);

    backend_callback_ = base::BindOnce(&CacheStorageRawApiKeys::BackendCallback,
                     weak_factory_.GetWeakPtr());
    CreateBackendAndRun(path, backend);
  }

  void CacheStorageRawApiKeys::BackendCallback() {
    if (backend_error_ != net::OK) {
      std::string error_message = "disk_cache backend failed with error " + std::to_string(backend_error_);
      SendErrKeysResponse(error_message);
      return;
    }
    keys_ = {};
    StartIteration();
  }

  void CacheStorageRawApiKeys::StartIteration() {
    if(!backend_) {
      // std::cout << "backend doesnt seem ok\n";
    }
    iterator_ = backend_->CreateIterator();
    auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();
    if (!task_runner) {
      SendErrKeysResponse("disk_cache iterator failed");
      return;
    }

    auto iterate_callback = base::BindOnce(
      &CacheStorageRawApiKeys::Iterate,
      weak_factory_.GetWeakPtr()
    );

    task_runner->PostTask(
      FROM_HERE,
      std::move(iterate_callback)
    );
  }

  void CacheStorageRawApiKeys::Iterate() {

    auto split_callback = base::SplitOnceCallback(
      base::BindOnce(
        &CacheStorageRawApiKeys::HandleCacheEntry,
        weak_factory_.GetWeakPtr()
      )
    );

    EntryResult result =
      iterator_->OpenNextEntry(std::move(split_callback.first));

    // ALWAYS???
    if (result.net_error() == net::ERR_IO_PENDING) {
      return;
    }

    std::move(split_callback.second).Run(std::move(result));

  }

  void CacheStorageRawApiKeys::HandleCacheEntry(EntryResult result) {
    if (result.net_error() == net::ERR_FAILED) {
      // This is the indicator that iteration is complete.
      auto response = std::make_unique<KeysResult>();
      response->status = "ok";
      response->keys = std::move(keys_);
      std::move(keys_callback_).Run(std::move(response));
      return;
    }

    if (result.net_error() < 0) {
      SendErrKeysResponse("disk_cache backend iteration failed with error " + std::to_string(result.net_error()));
      return;
    }

    ScopedEntryPtr entry(result.ReleaseEntry());
    keys_.push_back(entry->GetKey());
    auto iterate_callback = base::BindOnce(&CacheStorageRawApiKeys::Iterate, weak_factory_.GetWeakPtr());

    if (query_cache_recursive_depth_ <= kMaxQueryCacheRecursiveDepth) {
      query_cache_recursive_depth_ += 1;
      std::move(iterate_callback).Run();
      return;
    }

    query_cache_recursive_depth_ = 0;
    auto task_runner = base::SequencedTaskRunner::GetCurrentDefault();
    task_runner->PostTask(
      FROM_HERE,
      std::move(iterate_callback));
  }

  void CacheStorageRawApiKeys::SendErrKeysResponse(std::string error) {
    auto response = std::make_unique<KeysResult>();
    response->status = std::move(error);
    std::move(keys_callback_).Run(std::move(response));
  }
}
