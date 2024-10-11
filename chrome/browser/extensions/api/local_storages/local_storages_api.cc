// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/local_storages/local_storages_api.h"

#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "content/browser/cache_storage_raw/cache_storage_raw_control_wrapper.h"
#include "content/public/browser/storage_partition.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/local_storages.h"
#include "services/network/public/mojom/network_context.mojom.h"

namespace extensions {

class LocalStorageControl;

namespace Keys = api::local_storages::Keys;

LocalStoragesFunction::LocalStoragesFunction() {}
LocalStoragesFunction::~LocalStoragesFunction() {}

Profile* LocalStoragesFunction::GetProfile() const {
  return Profile::FromBrowserContext(browser_context());
}

void LocalStoragesKeysFunction::OnKeys(const ::storage::mojom::LocalStorageKeysResponsePtr remote_response) {
  if (remote_response->status == "ok") {
    Respond(ArgumentList(api::local_storages::Keys::Results::Create(remote_response->keys.value())));
  } else {
    Respond(Error(remote_response->status));
  }

  Release();
}

ExtensionFunction::ResponseAction LocalStoragesKeysFunction::Run() {
  AddRef();

  auto* ls_control = GetProfile()->GetDefaultStoragePartition()->GetLocalStorageControl();
  auto callback = base::BindOnce(&LocalStoragesKeysFunction::OnKeys, base::Unretained(this));
  ls_control->GetKeys(std::move(callback));

  return RespondLater();
}

void LocalStoragesGetEntryFunction::OnEntry(const ::storage::mojom::LocalStorageGetEntryResponsePtr remote_response) {
  if (remote_response->status == "ok") {
    Respond(ArgumentList(api::local_storages::GetEntry::Results::Create(*remote_response->value)));
  } else {
    if (remote_response->status == "not found") {
      // not found, return null
      Respond(WithArguments(base::Value()));
    } else {
      Respond(Error(remote_response->status));
    }
  }
  Release();
}

ExtensionFunction::ResponseAction LocalStoragesGetEntryFunction::Run() {
  std::optional<api::local_storages::GetEntry::Params> params = api::local_storages::GetEntry::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  AddRef();

  auto* ls_control = GetProfile()->GetDefaultStoragePartition()->GetLocalStorageControl();
  ls_control->GetEntry(params->key, base::BindOnce(&LocalStoragesGetEntryFunction::OnEntry, base::Unretained(this)));

  return RespondLater();
}

void LocalStoragesPutEntryFunction::OnEntrySaved(const ::std::string& status) {
  if (status == "ok") {
    Respond(NoArguments());
  } else {
    Respond(Error(status));
  }
  Release();
}

ExtensionFunction::ResponseAction LocalStoragesPutEntryFunction::Run() {
  std::optional<api::local_storages::PutEntry::Params> params = api::local_storages::PutEntry::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  AddRef();

  auto* ls_control = GetProfile()->GetDefaultStoragePartition()->GetLocalStorageControl();
  ls_control->PutEntry(params->key, params->value, base::BindOnce(&LocalStoragesPutEntryFunction::OnEntrySaved, base::Unretained(this)));

  return RespondLater();
}

void LocalStoragesDeleteEntryFunction::OnEntryDeleted(const ::std::string& status) {
  if (status == "ok") {
    Respond(NoArguments());
  } else {
    Respond(Error(status));
  }
  Release();
}

ExtensionFunction::ResponseAction LocalStoragesDeleteEntryFunction::Run() {
  std::optional<api::local_storages::DeleteEntry::Params> params = api::local_storages::DeleteEntry::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  AddRef();

  auto* ls_control = GetProfile()->GetDefaultStoragePartition()->GetLocalStorageControl();
  ls_control->DeleteEntry(params->key, base::BindOnce(&LocalStoragesDeleteEntryFunction::OnEntryDeleted, base::Unretained(this)));

  return RespondLater();
}

ExtensionFunction::ResponseAction LocalStoragesFlushFunction::Run() {
  auto* ls_control = GetProfile()->GetDefaultStoragePartition()->GetLocalStorageControl();
  ls_control->Flush();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction LocalStoragesPurgeMemoryFunction::Run() {
  auto* ls_control = GetProfile()->GetDefaultStoragePartition()->GetLocalStorageControl();
  ls_control->PurgeMemory();
  return RespondNow(NoArguments());
}

}  // namespace extensions
