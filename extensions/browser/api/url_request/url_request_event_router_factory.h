// Copyright 2023 The Chromium Authors
// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_URL_REQUEST_URL_REQUEST_EVENT_ROUTER_FACTORY_H_
#define EXTENSIONS_BROWSER_API_URL_REQUEST_URL_REQUEST_EVENT_ROUTER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

class UrlRequestEventRouter;

class UrlRequestEventRouterFactory : public BrowserContextKeyedServiceFactory {
 public:
  UrlRequestEventRouterFactory(const UrlRequestEventRouterFactory&) = delete;
  UrlRequestEventRouterFactory& operator=(const UrlRequestEventRouterFactory&) =
      delete;

  static UrlRequestEventRouter* GetForBrowserContext(
      content::BrowserContext* context);
  static UrlRequestEventRouterFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<UrlRequestEventRouterFactory>;

  UrlRequestEventRouterFactory();
  ~UrlRequestEventRouterFactory() override;

  // BrowserContextKeyedServiceFactory implementation
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_URL_REQUEST_URL_REQUEST_EVENT_ROUTER_FACTORY_H_
