// Copyright 2019 The Chromium Authors
// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/url_request/url_request_permission_helper.h"

#include "base/no_destructor.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/process_map.h"
#include "extensions/browser/process_map_factory.h"

namespace extensions {

UrlRequestPermissionHelper::UrlRequestPermissionHelper(content::BrowserContext* context)
    : browser_context_(context),
      extension_registry_(ExtensionRegistry::Get(context)) {
  // Ensure the dependency is constructed.
  ProcessMap::Get(browser_context_);
}

UrlRequestPermissionHelper::~UrlRequestPermissionHelper() = default;

// static
UrlRequestPermissionHelper* UrlRequestPermissionHelper::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<UrlRequestPermissionHelper>::Get(context);
}

// static
BrowserContextKeyedAPIFactory<UrlRequestPermissionHelper>*
 UrlRequestPermissionHelper::GetFactoryInstance() {
  static base::NoDestructor<BrowserContextKeyedAPIFactory<UrlRequestPermissionHelper>>
      instance;
  return instance.get();
}

bool UrlRequestPermissionHelper::ShouldHideBrowserNetworkRequest(
    const UrlRequestInfo& request) const {
  // return ExtensionsAPIClient::Get()->ShouldHideBrowserNetworkRequest(
  //     browser_context_, request);
  return false;
}

bool UrlRequestPermissionHelper::CanCrossIncognito(const Extension* extension) const {
  return extensions::util::CanCrossIncognito(extension, browser_context_);
}

template <>
void BrowserContextKeyedAPIFactory<
    UrlRequestPermissionHelper>::DeclareFactoryDependencies() {
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(ProcessMapFactory::GetInstance());
  // Used in CanCrossIncognito().
  DependsOn(ExtensionPrefsFactory::GetInstance());
  // For ShouldHideBrowserNetworkRequest().
  for (auto* factory : ExtensionsAPIClient::Get()->GetFactoryDependencies())
    DependsOn(factory);
}

}  // namespace extensions
