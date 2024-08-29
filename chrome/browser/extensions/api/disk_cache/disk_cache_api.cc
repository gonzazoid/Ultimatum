// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/disk_cache/disk_cache_api.h"
#include "chrome/common/extensions/api/disk_cache.h"
#include "content/browser/code_cache/generated_code_cache_context.h"

#include <iostream>
#include <memory>
#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

#include "content/browser/cache_storage_raw/cache_storage_raw_control_wrapper.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/network_context.mojom.h"

#include "net/http/http_cache.h"

namespace extensions {

namespace Keys = api::disk_cache::Keys;

DiskCacheAPI::DiskCacheAPI(content::BrowserContext* context)
    : browser_context_(context) {}

DiskCacheAPI::~DiskCacheAPI() {}

DiskCacheFunction::DiskCacheFunction() {}
DiskCacheFunction::~DiskCacheFunction() {}

void DiskCacheFunction::Init() {
  storage::mojom::CacheStorageRawControl* control = GetProfile()->GetDefaultStoragePartition()->GetCacheStorageRawControl();
  mojo::PendingRemote<blink::mojom::CacheStorageRaw> remote;
  control->AddReceiver(remote.InitWithNewPipeAndPassReceiver());
  cache_storage_.Bind(std::move(remote));
}

void DiskCacheFunction::GetPath(std::string target, base::OnceCallback<void (const base::FilePath&)> callback) {
  base::FilePath path;

  if (target == "js_code") {
    path = GetProfile()->GetDefaultStoragePartition()->GetGeneratedCodeCacheContext()->generated_js_code_cache_path();
  }
  if (target == "wasm_code") {
    path = GetProfile()->GetDefaultStoragePartition()->GetGeneratedCodeCacheContext()->generated_wasm_code_cache_path();
  }
  if (target == "webui_js_code") {
    path = GetProfile()->GetDefaultStoragePartition()->GetGeneratedCodeCacheContext()->generated_webui_js_code_cache_path();
  }

  // return even if not found
  std::move(callback).Run(path);
}

Profile* DiskCacheFunction::GetProfile() const {
  return Profile::FromBrowserContext(browser_context());
}

void DiskCacheKeysFunction::OnKeys(const ::network::mojom::DiskCacheKeysResponsePtr remote_response) {
  if (remote_response->status != "ok") {
    Respond(Error(remote_response->status));
  } else {
    Respond(ArgumentList(api::disk_cache::Keys::Results::Create(remote_response->keys.value())));
  }
  Release();
}

void DiskCacheKeysFunction::OnPath(const base::FilePath& path) {
  if (path.empty()) {
    Respond(Error("unknown storage"));
    return;
  }
  cache_storage_->Keys(path, base::BindOnce(&DiskCacheKeysFunction::OnKeys, base::Unretained(this)));
}

ExtensionFunction::ResponseAction DiskCacheKeysFunction::Run() {
  std::optional<api::disk_cache::Keys::Params> params = api::disk_cache::Keys::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Init();

  if (params->storage == "http") {
    auto* network_context = GetProfile()->GetDefaultStoragePartition()->GetNetworkContext();
    network_context->GetHttpCacheKeys(base::BindOnce(&DiskCacheKeysFunction::OnKeys, base::Unretained(this)));
  } else {
    auto callback = base::BindOnce(&DiskCacheKeysFunction::OnPath, base::Unretained(this));
    GetPath(params->storage, std::move(callback));
  }

  AddRef();
  return RespondLater();
}

void DiskCacheGetEntryFunction::OnEntry(const ::network::mojom::DiskCacheEntryResponsePtr remote_response) {
  if (remote_response->status != "ok") {
    if (remote_response->status == "not found") {
      // not found, return null
      Respond(WithArguments(base::Value()));
    } else {
      Respond(Error(remote_response->status));
    }
    Release();
    return;
  }

  api::disk_cache::CacheRawEntry entry;
  entry.key = std::move(remote_response->entry->key);
  entry.stream0 = std::move(remote_response->entry->stream0);
  entry.stream1 = std::move(remote_response->entry->stream1);
  if (remote_response->entry->ranges.has_value()) {
    entry.ranges = std::vector<api::disk_cache::CacheRawRange>();
    for (auto& it : remote_response->entry->ranges.value()) {
      auto range = api::disk_cache::CacheRawRange();
      range.offset = it->offset;
      range.length = it->length;
      entry.ranges.value().push_back(std::move(range));
    }
  } else {
    entry.stream2 = std::move(remote_response->entry->stream2);
  }
  Respond(ArgumentList(api::disk_cache::GetEntry::Results::Create(entry)));
  Release();
}

void DiskCacheGetEntryFunction::OnPath(std::string key, const base::FilePath& path) {
  if (path.empty()) {
    Respond(Error("unknown storage"));
    return;
  }
  cache_storage_->GetCacheEntry(path, key, base::BindOnce(&DiskCacheGetEntryFunction::OnEntry, base::Unretained(this)));
}

ExtensionFunction::ResponseAction DiskCacheGetEntryFunction::Run() {
  std::optional<api::disk_cache::GetEntry::Params> params = api::disk_cache::GetEntry::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Init();

  if (params->storage == "http") {
    auto* network_context = GetProfile()->GetDefaultStoragePartition()->GetNetworkContext();
    network_context->GetHttpCacheEntry(
      params->key,
      base::BindOnce(&DiskCacheGetEntryFunction::OnEntry, base::Unretained(this))
    );
  } else {
    auto callback = base::BindOnce(&DiskCacheGetEntryFunction::OnPath, base::Unretained(this), params->key);
    GetPath(params->storage, std::move(callback));
  }

  AddRef();
  return RespondLater();
}

void DiskCachePutEntryFunction::OnEntrySaved(const ::std::string& status) {
  if (status == "ok") {
    Respond(NoArguments());
  } else {
    Respond(Error(status));
  }
  Release();
}

void DiskCachePutEntryFunction::OnPath(::network::mojom::DiskCacheEntryPtr entry, const base::FilePath& path) {
  if (path.empty()) {
    Respond(Error("unknown storage"));
    return;
  }
  cache_storage_->PutCacheEntry(
    path,
    std::move(entry),
    base::BindOnce(&DiskCachePutEntryFunction::OnEntrySaved, base::Unretained(this))
  );
}

ExtensionFunction::ResponseAction DiskCachePutEntryFunction::Run() {
  std::optional<api::disk_cache::PutEntry::Params> params = api::disk_cache::PutEntry::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Init();

  auto cache_entry = ::network::mojom::DiskCacheEntry::New();
  cache_entry->key = params->entry.key;
  cache_entry->stream0 = std::move(params->entry.stream0);
  cache_entry->stream1 = std::move(params->entry.stream1);
  if (params->entry.ranges.has_value()) {
    auto ranges = std::vector<::network::mojom::DiskCacheEntryRangePtr>();
    for (auto& entry : params->entry.ranges.value()) {
      auto newEntry = ::network::mojom::DiskCacheEntryRange::New(entry.offset, entry.length);
      ranges.push_back(std::move(newEntry));
    }
    cache_entry->ranges = std::move(ranges);
  } else {
    cache_entry->stream2 = std::move(params->entry.stream2);
  }

  if (params->storage == "http") {
    auto* network_context = GetProfile()->GetDefaultStoragePartition()->GetNetworkContext();
    network_context->PutHttpCacheEntry(
      std::move(cache_entry),
      base::BindOnce(&DiskCachePutEntryFunction::OnEntrySaved, base::Unretained(this)));
  } else {
    auto callback = base::BindOnce(&DiskCachePutEntryFunction::OnPath, base::Unretained(this), std::move(cache_entry));
    GetPath(params->storage, std::move(callback));
  }

  AddRef();
  return RespondLater();
}

void DiskCacheDeleteEntryFunction::OnEntryDeleted(const ::std::string& status) {
  if (status == "ok") {
    Respond(NoArguments());
  } else {
    Respond(Error(status));
  }
  Release();
}

void DiskCacheDeleteEntryFunction::OnPath(std::string key, const base::FilePath& path) {
  if (path.empty()) {
    Respond(Error("unknown storage"));
    return;
  }
  cache_storage_->DeleteCacheEntry(
    path,
    key,
    base::BindOnce(&DiskCacheDeleteEntryFunction::OnEntryDeleted, base::Unretained(this))
  );
}

ExtensionFunction::ResponseAction DiskCacheDeleteEntryFunction::Run() {
  std::optional<api::disk_cache::DeleteEntry::Params> params = api::disk_cache::DeleteEntry::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  Init();

  if (params->storage == "http") {
    auto* network_context = GetProfile()->GetDefaultStoragePartition()->GetNetworkContext();
    network_context->DeleteHttpCacheEntry(
      params->key,
      base::BindOnce(&DiskCacheDeleteEntryFunction::OnEntryDeleted, base::Unretained(this)));
  } else {
    auto callback = base::BindOnce(&DiskCacheDeleteEntryFunction::OnPath, base::Unretained(this), params->key);
    GetPath(params->storage, std::move(callback));
  }

  AddRef();
  return RespondLater();
}

}  // namespace extensions
