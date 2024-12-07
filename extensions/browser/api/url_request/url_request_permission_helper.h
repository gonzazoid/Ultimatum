// Copyright 2019 The Chromium Authors
// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_URL_REQUEST_PERMISSION_HELPER_H_
#define EXTENSIONS_BROWSER_API_URL_REQUEST_PERMISSION_HELPER_H_

#include "base/memory/raw_ptr.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/process_map.h"

namespace extensions {
class ExtensionRegistry;
class ProcessMap;
struct UrlRequestInfo;

// Intermediate keyed service used to declare dependencies on other services
// that are needed for UrlRequest permissions.
class UrlRequestPermissionHelper : public BrowserContextKeyedAPI {
 public:
  explicit UrlRequestPermissionHelper(content::BrowserContext* context);

  UrlRequestPermissionHelper(const UrlRequestPermissionHelper&) = delete;
  UrlRequestPermissionHelper& operator=(const UrlRequestPermissionHelper&) = delete;

  ~UrlRequestPermissionHelper() override;

  // Convenience method to get the PermissionHelper for a profile.
  static UrlRequestPermissionHelper* Get(content::BrowserContext* context);

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<UrlRequestPermissionHelper>* GetFactoryInstance();

  bool ShouldHideBrowserNetworkRequest(const UrlRequestInfo& request) const;
  bool CanCrossIncognito(const Extension* extension) const;

  const ProcessMap* process_map() const {
    return ProcessMap::Get(browser_context_);
  }

  const ExtensionRegistry* extension_registry() const {
    return extension_registry_;
  }

 private:
  friend class BrowserContextKeyedAPIFactory<UrlRequestPermissionHelper>;

  const raw_ptr<content::BrowserContext> browser_context_;
  const raw_ptr<ExtensionRegistry> extension_registry_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "UrlRequestPermissionHelper"; }
  static const bool kServiceRedirectedInIncognito = true;
};

template <>
void BrowserContextKeyedAPIFactory<
    UrlRequestPermissionHelper>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_URL_REQUEST_PERMISSION_HELPER_H_
