// Copyright 2017 The Chromium Authors
// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/browser/api/url_request/url_request_resource_type.h"

#include <string_view>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"

namespace extensions {

namespace {

constexpr struct {
  const char* const name;
  const UrlRequestResourceType type;
} kResourceTypes[] = {
    {"main_frame", UrlRequestResourceType::MAIN_FRAME},
    {"sub_frame", UrlRequestResourceType::SUB_FRAME},
    {"stylesheet", UrlRequestResourceType::STYLESHEET},
    {"script", UrlRequestResourceType::SCRIPT},
    {"image", UrlRequestResourceType::IMAGE},
    {"font", UrlRequestResourceType::FONT},
    {"object", UrlRequestResourceType::OBJECT},
    {"xmlhttprequest", UrlRequestResourceType::XHR},
    {"ping", UrlRequestResourceType::PING},
    {"csp_report", UrlRequestResourceType::CSP_REPORT},
    {"media", UrlRequestResourceType::MEDIA},
    {"websocket", UrlRequestResourceType::WEB_SOCKET},
    {"webtransport", UrlRequestResourceType::WEB_TRANSPORT},
    {"webbundle", UrlRequestResourceType::WEBBUNDLE},
    {"other", UrlRequestResourceType::OTHER},
};

constexpr size_t kResourceTypesLength = std::size(kResourceTypes);

static_assert(kResourceTypesLength ==
                  base::strict_cast<size_t>(UrlRequestResourceType::OTHER) + 1,
              "Each UrlRequestResourceType should have a string name.");

}  // namespace

UrlRequestResourceType ToUrlRequestResourceType(
    const network::ResourceRequest& request,
    bool is_download) {
  if (request.url.SchemeIsWSOrWSS())
    return UrlRequestResourceType::WEB_SOCKET;
  if (is_download)
    return UrlRequestResourceType::OTHER;
  if (request.is_fetch_like_api) {
    // This must be checked before `request.keepalive` check below, because
    // currently Fetch keepAlive is not reported as ping.
    // See https://crbug.com/611453 for more details.
    return UrlRequestResourceType::XHR;
  }

  switch (request.destination) {
    case network::mojom::RequestDestination::kDocument:
      return UrlRequestResourceType::MAIN_FRAME;
    case network::mojom::RequestDestination::kIframe:
    case network::mojom::RequestDestination::kFrame:
    case network::mojom::RequestDestination::kFencedframe:
      return UrlRequestResourceType::SUB_FRAME;
    case network::mojom::RequestDestination::kStyle:
    case network::mojom::RequestDestination::kXslt:
      return UrlRequestResourceType::STYLESHEET;
    case network::mojom::RequestDestination::kScript:
    // TODO(crbug.com/41484304): Consider adding a new
    // webRequest.ResourceType for JSON requests modules.
    // TODO (gonzazoid) keep eye on it
    case network::mojom::RequestDestination::kJson:
      return UrlRequestResourceType::SCRIPT;
    case network::mojom::RequestDestination::kImage:
      return UrlRequestResourceType::IMAGE;
    case network::mojom::RequestDestination::kFont:
      return UrlRequestResourceType::FONT;
    case network::mojom::RequestDestination::kObject:
    case network::mojom::RequestDestination::kEmbed:
      return UrlRequestResourceType::OBJECT;
    case network::mojom::RequestDestination::kAudio:
    case network::mojom::RequestDestination::kTrack:
    case network::mojom::RequestDestination::kVideo:
      return UrlRequestResourceType::MEDIA;
    case network::mojom::RequestDestination::kWorker:
    case network::mojom::RequestDestination::kSharedWorker:
    case network::mojom::RequestDestination::kServiceWorker:
    case network::mojom::RequestDestination::kSharedStorageWorklet:
      return UrlRequestResourceType::SCRIPT;
    case network::mojom::RequestDestination::kReport:
      return UrlRequestResourceType::CSP_REPORT;
    case network::mojom::RequestDestination::kEmpty:
      // https://fetch.spec.whatwg.org/#concept-request-destination
      if (request.keepalive)
        return UrlRequestResourceType::PING;
      return UrlRequestResourceType::OTHER;
    case network::mojom::RequestDestination::kWebBundle:
      return UrlRequestResourceType::WEBBUNDLE;
    case network::mojom::RequestDestination::kAudioWorklet:
    case network::mojom::RequestDestination::kManifest:
    case network::mojom::RequestDestination::kPaintWorklet:
    case network::mojom::RequestDestination::kWebIdentity:
    // The compression dictionary has not been exposed to extensions yet.
    // We could do so if the need arises.
    case network::mojom::RequestDestination::kDictionary:
    case network::mojom::RequestDestination::kSpeculationRules:
      return UrlRequestResourceType::OTHER;
  }
  NOTREACHED_IN_MIGRATION();
  return UrlRequestResourceType::OTHER;
}

const char* UrlRequestResourceTypeToString(UrlRequestResourceType type) {
  size_t index = base::strict_cast<size_t>(type);
  DCHECK_LT(index, kResourceTypesLength);
  DCHECK_EQ(kResourceTypes[index].type, type);
  return kResourceTypes[index].name;
}

bool ParseUrlRequestResourceType(std::string_view text,
                                 UrlRequestResourceType* type) {
  for (size_t i = 0; i < kResourceTypesLength; ++i) {
    if (text == kResourceTypes[i].name) {
      *type = kResourceTypes[i].type;
      DCHECK_EQ(static_cast<UrlRequestResourceType>(i), *type);
      return true;
    }
  }
  return false;
}

}  // namespace extensions
