// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/hsts_cache/hsts_cache_api.h"
#include "chrome/common/extensions/api/hsts_cache.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extensions_browser_client.h"

#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace extensions {

namespace Keys = api::hsts_cache::Keys;

HstsCacheFunction::HstsCacheFunction() {}
HstsCacheFunction::~HstsCacheFunction() {}

Profile* HstsCacheFunction::GetProfile() const {
  return Profile::FromBrowserContext(browser_context());
}

void HstsCacheKeysFunction::OnKeys(const ::network::mojom::HstsCacheKeysResponsePtr remote_response) {
  if (remote_response->status != "ok") {
    Respond(Error(remote_response->status));
  } else {
    Respond(ArgumentList(api::hsts_cache::Keys::Results::Create(remote_response->keys.value())));
  }
  Release();
}

ExtensionFunction::ResponseAction HstsCacheKeysFunction::Run() {
  auto* network_context = GetProfile()->GetDefaultStoragePartition()->GetNetworkContext();
  network_context->GetHstsCacheKeys(base::BindOnce(&HstsCacheKeysFunction::OnKeys, base::Unretained(this)));

  AddRef();
  return RespondLater();
}

void HstsCacheGetEntryFunction::OnEntry(const ::network::mojom::HstsCacheGetEntryResponsePtr remote_response) {
  if (remote_response->status != "ok") {
    if (remote_response->status == "not found") {
      // not found, return null
      Respond(WithArguments(base::Value()));
    } else {
      Respond(Error(remote_response->status));
    }
  } else {
    api::hsts_cache::HstsCacheEntry entry;
    entry.key = std::move(remote_response->entry->key);
    entry.upgrade_mode = remote_response->entry->upgrade_mode;
    entry.last_observed = remote_response->entry->last_observed;
    entry.expiry = remote_response->entry->expiry;
    entry.include_subdomains = remote_response->entry->include_subdomains;

    Respond(ArgumentList(api::hsts_cache::GetEntry::Results::Create(entry)));
  }
  Release();
}

ExtensionFunction::ResponseAction HstsCacheGetEntryFunction::Run() {
  std::optional<api::hsts_cache::GetEntry::Params> params = api::hsts_cache::GetEntry::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  auto* network_context = GetProfile()->GetDefaultStoragePartition()->GetNetworkContext();
  network_context->GetHstsCacheEntry(params->key, base::BindOnce(&HstsCacheGetEntryFunction::OnEntry, base::Unretained(this)));

  AddRef();
  return RespondLater();
}

void HstsCachePutEntryFunction::OnEntrySaved(const ::std::string& status) {
  if (status == "ok") {
    Respond(NoArguments());
  } else {
    Respond(Error(status));
  }
  Release();
}

ExtensionFunction::ResponseAction HstsCachePutEntryFunction::Run() {
  std::optional<api::hsts_cache::PutEntry::Params> params = api::hsts_cache::PutEntry::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  AddRef();

  auto hsts_entry = ::network::mojom::HstsCacheEntry::New();
  hsts_entry->key = params->entry.key;
  hsts_entry->last_observed = params->entry.last_observed;
  hsts_entry->expiry = params->entry.expiry;
  hsts_entry->upgrade_mode = params->entry.upgrade_mode;
  hsts_entry->include_subdomains = params->entry.include_subdomains;

  auto* network_context = GetProfile()->GetDefaultStoragePartition()->GetNetworkContext();
  network_context->PutHstsCacheEntry(
    std::move(hsts_entry),
    base::BindOnce(&HstsCachePutEntryFunction::OnEntrySaved, base::Unretained(this))
  );

  return RespondLater();
}

void HstsCacheDeleteEntryFunction::OnEntryDeleted(const ::std::string& status) {
  if (status == "ok") {
    Respond(NoArguments());
  } else {
    Respond(Error(status));
  }
  Release();
}

ExtensionFunction::ResponseAction HstsCacheDeleteEntryFunction::Run() {
  std::optional<api::hsts_cache::DeleteEntry::Params> params = api::hsts_cache::DeleteEntry::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  AddRef();

  auto* network_context = GetProfile()->GetDefaultStoragePartition()->GetNetworkContext();
  network_context->DeleteHstsCacheEntry(
    std::move(params->key),
    base::BindOnce(&HstsCacheDeleteEntryFunction::OnEntryDeleted, base::Unretained(this))
  );

  return RespondLater();
}

}  // namespace extensions
