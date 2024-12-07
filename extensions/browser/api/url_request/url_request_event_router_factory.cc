// Copyright 2023 The Chromium Authors
// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/url_request/url_request_event_router_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/url_request/extension_url_request_event_router.h"
#include "extensions/browser/api/url_request/url_request_permission_helper.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_map_factory.h"

using content::BrowserContext;

namespace extensions {

// static
UrlRequestEventRouter* UrlRequestEventRouterFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<UrlRequestEventRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
UrlRequestEventRouterFactory* UrlRequestEventRouterFactory::GetInstance() {
  return base::Singleton<UrlRequestEventRouterFactory>::get();
}

UrlRequestEventRouterFactory::UrlRequestEventRouterFactory()
    : BrowserContextKeyedServiceFactory(
          "UrlRequestEventRouter",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(EventRouterFactory::GetInstance());
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(UrlRequestPermissionHelper::GetFactoryInstance());
  DependsOn(ProcessMapFactory::GetInstance());
}

UrlRequestEventRouterFactory::~UrlRequestEventRouterFactory() = default;

KeyedService* UrlRequestEventRouterFactory::BuildServiceInstanceFor(
    BrowserContext* context) const {
  return new UrlRequestEventRouter(context);
}

BrowserContext* UrlRequestEventRouterFactory::GetBrowserContextToUse(
    BrowserContext* context) const {
  // UrlRequestAPI shares an instance between regular and incognito profiles,
  // so this must do the same.
  return ExtensionsBrowserClient::Get()->GetContextRedirectedToOriginal(
      context, /*force_guest_profile=*/true);
}

bool UrlRequestEventRouterFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace extensions
