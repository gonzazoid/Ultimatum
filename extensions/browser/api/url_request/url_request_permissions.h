// Copyright 2012 The Chromium Authors
// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_URL_REQUEST_URL_REQUEST_PERMISSIONS_H_
#define EXTENSIONS_BROWSER_API_URL_REQUEST_URL_REQUEST_PERMISSIONS_H_

#include <optional>
#include <string>

#include "extensions/browser/api/url_request/url_request_resource_type.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/permissions/permissions_data.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

class GURL;

namespace extensions {
class UrlRequestPermissionHelper;
struct UrlRequestInfo;
}

namespace url {
class Origin;
}

// This class is used to test whether extensions may modify web requests. It
// should be used on the IO thread.
class UrlRequestPermissions {
 public:
  // Different host permission checking modes for CanExtensionAccessURL.
  enum HostPermissionsCheck {
    DO_NOT_CHECK_HOST = 0,  // No check.
    // Permission needed for given request URL.
    // TODO(karandeepb): Remove this checking mode.
    REQUIRE_HOST_PERMISSION_FOR_URL,
    // Same as REQUIRE_HOST_PERMISSION_FOR_URL but sub-resource requests will
    // also need access to the request initiator.
    REQUIRE_HOST_PERMISSION_FOR_URL_AND_INITIATOR,
    // Permission needed for <all_urls>.
    REQUIRE_ALL_URLS
  };

  UrlRequestPermissions() = delete;
  UrlRequestPermissions(const UrlRequestPermissions&) = delete;
  UrlRequestPermissions& operator=(const UrlRequestPermissions&) = delete;

  // Returns true if the request shall not be reported to extensions.
  static bool HideRequest(extensions::UrlRequestPermissionHelper* permission_helper,
                          const extensions::UrlRequestInfo& request);

  // |host_permission_check| controls how permissions are checked with regard to
  // |url| and |initiator| if an initiator exists.
  static extensions::PermissionsData::PageAccess CanExtensionAccessURL(
      extensions::UrlRequestPermissionHelper* permission_helper,
      const extensions::ExtensionId& extension_id,
      const GURL& url,
      int tab_id,
      bool crosses_incognito,
      HostPermissionsCheck host_permissions_check,
      const std::optional<url::Origin>& initiator,
      extensions::UrlRequestResourceType url_request_type);

  static bool CanExtensionAccessInitiator(
      extensions::UrlRequestPermissionHelper* permission_helper,
      const extensions::ExtensionId extension_id,
      const std::optional<url::Origin>& initiator,
      int tab_id,
      bool crosses_incognito);
};

#endif  // EXTENSIONS_BROWSER_API_URL_REQUEST_URL_REQUEST_PERMISSIONS_H_
