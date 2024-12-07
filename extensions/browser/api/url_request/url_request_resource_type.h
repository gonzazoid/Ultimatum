// Copyright 2017 The Chromium Authors
// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_URL_REQUEST_URL_REQUEST_RESOURCE_TYPE_H_
#define EXTENSIONS_BROWSER_API_URL_REQUEST_URL_REQUEST_RESOURCE_TYPE_H_

#include <stdint.h>

#include <string_view>

#include "services/network/public/cpp/resource_request.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace extensions {

// Enumerates all resource/request types that UrlRequest API cares about.
enum class UrlRequestResourceType : uint8_t {
  MAIN_FRAME,
  SUB_FRAME,
  STYLESHEET,
  SCRIPT,
  IMAGE,
  FONT,
  OBJECT,
  XHR,
  PING,
  CSP_REPORT,
  MEDIA,
  WEB_SOCKET,
  WEB_TRANSPORT,
  WEBBUNDLE,

  OTHER,  // The type is unknown, or differs from all the above.
};

UrlRequestResourceType ToUrlRequestResourceType(
    const network::ResourceRequest& request,
    bool is_download);

// Returns a string representation of |type|.
const char* UrlRequestResourceTypeToString(UrlRequestResourceType type);

// Finds a |type| such that its string representation equals to |text|. Returns
// true iff the type is found.
bool ParseUrlRequestResourceType(std::string_view text,
                                 UrlRequestResourceType* type);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_URL_REQUEST_URL_REQUEST_RESOURCE_TYPE_H_
