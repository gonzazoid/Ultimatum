// Copyright 2012 The Chromium Authors
// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/url_request/url_request_api_helpers.h"

#include <stddef.h>
#include <stdint.h>

#include <cmath>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/fixed_flat_set.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/ranges/algorithm.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "components/web_cache/browser/web_cache_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/url_request/url_request_api_constants.h"
#include "extensions/browser/api/url_request/url_request_info.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension_id.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_util.h"
#include "net/log/net_log_event_type.h"
#include "services/network/public/cpp/features.h"
#include "url/url_constants.h"

// TODO(battre): move all static functions into an anonymous namespace at the
// top of this file.

using base::Time;

namespace keys = extension_url_request_api_constants;
namespace url_request = extensions::api::url_request;

namespace extension_url_request_api_helpers {

namespace {

void ClearCacheOnNavigationOnUI() {
  extensions::ExtensionsBrowserClient::Get()->ClearBackForwardCache();
  web_cache::WebCacheManager::GetInstance()->ClearCacheOnNavigation();
}

constexpr auto kRequestHeaderEntries =
    base::MakeFixedFlatMap<std::string_view, RequestHeaderType>(
        {{"accept", RequestHeaderType::kAccept},
         {"accept-charset", RequestHeaderType::kAcceptCharset},
         {"accept-encoding", RequestHeaderType::kAcceptEncoding},
         {"accept-language", RequestHeaderType::kAcceptLanguage},
         {"access-control-request-headers",
          RequestHeaderType::kAccessControlRequestHeaders},
         {"access-control-request-method",
          RequestHeaderType::kAccessControlRequestMethod},
         {"authorization", RequestHeaderType::kAuthorization},
         {"cache-control", RequestHeaderType::kCacheControl},
         {"connection", RequestHeaderType::kConnection},
         {"content-encoding", RequestHeaderType::kContentEncoding},
         {"content-language", RequestHeaderType::kContentLanguage},
         {"content-length", RequestHeaderType::kContentLength},
         {"content-location", RequestHeaderType::kContentLocation},
         {"content-type", RequestHeaderType::kContentType},
         {"cookie", RequestHeaderType::kCookie},
         {"date", RequestHeaderType::kDate},
         {"dnt", RequestHeaderType::kDnt},
         {"early-data", RequestHeaderType::kEarlyData},
         {"expect", RequestHeaderType::kExpect},
         {"forwarded", RequestHeaderType::kForwarded},
         {"from", RequestHeaderType::kFrom},
         {"host", RequestHeaderType::kHost},
         {"if-match", RequestHeaderType::kIfMatch},
         {"if-modified-since", RequestHeaderType::kIfModifiedSince},
         {"if-none-match", RequestHeaderType::kIfNoneMatch},
         {"if-range", RequestHeaderType::kIfRange},
         {"if-unmodified-since", RequestHeaderType::kIfUnmodifiedSince},
         {"keep-alive", RequestHeaderType::kKeepAlive},
         {"origin", RequestHeaderType::kOrigin},
         {"pragma", RequestHeaderType::kPragma},
         {"proxy-authorization", RequestHeaderType::kProxyAuthorization},
         {"proxy-connection", RequestHeaderType::kProxyConnection},
         {"range", RequestHeaderType::kRange},
         {"referer", RequestHeaderType::kReferer},
         {"te", RequestHeaderType::kTe},
         {"transfer-encoding", RequestHeaderType::kTransferEncoding},
         {"upgrade", RequestHeaderType::kUpgrade},
         {"upgrade-insecure-requests",
          RequestHeaderType::kUpgradeInsecureRequests},
         {"user-agent", RequestHeaderType::kUserAgent},
         {"via", RequestHeaderType::kVia},
         {"warning", RequestHeaderType::kWarning},
         {"x-forwarded-for", RequestHeaderType::kXForwardedFor},
         {"x-forwarded-host", RequestHeaderType::kXForwardedHost},
         {"x-forwarded-proto", RequestHeaderType::kXForwardedProto}});

constexpr bool IsValidHeaderName(std::string_view str) {
  for (char ch : str) {
    if ((ch < 'a' || ch > 'z') && ch != '-')
      return false;
  }
  return true;
}

template <typename T>
constexpr bool ValidateHeaderEntries(const T& entries) {
  for (const auto& entry : entries) {
    if (!IsValidHeaderName(entry.first))
      return false;
  }
  return true;
}

// All entries other than kOther and kNone are mapped.
// sec-origin-policy was removed.
// So -2 is -1 for the count of the enums, and -1 for the removed
// sec-origin-policy which does not have a corresponding entry in
// kRequestHeaderEntries but does contribute to RequestHeaderType::kMaxValue.
static_assert(static_cast<size_t>(RequestHeaderType::kMaxValue) - 2 ==
                  kRequestHeaderEntries.size(),
              "Invalid number of request header entries");

static_assert(ValidateHeaderEntries(kRequestHeaderEntries),
              "Invalid request header entries");

constexpr auto kResponseHeaderEntries =
    base::MakeFixedFlatMap<std::string_view, ResponseHeaderType>({
        {"accept-patch", ResponseHeaderType::kAcceptPatch},
        {"accept-ranges", ResponseHeaderType::kAcceptRanges},
        {"access-control-allow-credentials",
         ResponseHeaderType::kAccessControlAllowCredentials},
        {"access-control-allow-headers",
         ResponseHeaderType::kAccessControlAllowHeaders},
        {"access-control-allow-methods",
         ResponseHeaderType::kAccessControlAllowMethods},
        {"access-control-allow-origin",
         ResponseHeaderType::kAccessControlAllowOrigin},
        {"access-control-expose-headers",
         ResponseHeaderType::kAccessControlExposeHeaders},
        {"access-control-max-age", ResponseHeaderType::kAccessControlMaxAge},
        {"age", ResponseHeaderType::kAge},
        {"allow", ResponseHeaderType::kAllow},
        {"alt-svc", ResponseHeaderType::kAltSvc},
        {"cache-control", ResponseHeaderType::kCacheControl},
        {"clear-site-data", ResponseHeaderType::kClearSiteData},
        {"connection", ResponseHeaderType::kConnection},
        {"content-disposition", ResponseHeaderType::kContentDisposition},
        {"content-encoding", ResponseHeaderType::kContentEncoding},
        {"content-language", ResponseHeaderType::kContentLanguage},
        {"content-length", ResponseHeaderType::kContentLength},
        {"content-location", ResponseHeaderType::kContentLocation},
        {"content-range", ResponseHeaderType::kContentRange},
        {"content-security-policy", ResponseHeaderType::kContentSecurityPolicy},
        {"content-security-policy-report-only",
         ResponseHeaderType::kContentSecurityPolicyReportOnly},
        {"content-type", ResponseHeaderType::kContentType},
        {"date", ResponseHeaderType::kDate},
        {"etag", ResponseHeaderType::kETag},
        {"expect-ct", ResponseHeaderType::kExpectCT},
        {"expires", ResponseHeaderType::kExpires},
        {"feature-policy", ResponseHeaderType::kFeaturePolicy},
        {"keep-alive", ResponseHeaderType::kKeepAlive},
        {"large-allocation", ResponseHeaderType::kLargeAllocation},
        {"last-modified", ResponseHeaderType::kLastModified},
        {"location", ResponseHeaderType::kLocation},
        {"pragma", ResponseHeaderType::kPragma},
        {"proxy-authenticate", ResponseHeaderType::kProxyAuthenticate},
        {"proxy-connection", ResponseHeaderType::kProxyConnection},
        {"public-key-pins", ResponseHeaderType::kPublicKeyPins},
        {"public-key-pins-report-only",
         ResponseHeaderType::kPublicKeyPinsReportOnly},
        {"referrer-policy", ResponseHeaderType::kReferrerPolicy},
        {"refresh", ResponseHeaderType::kRefresh},
        {"retry-after", ResponseHeaderType::kRetryAfter},
        {"sec-websocket-accept", ResponseHeaderType::kSecWebSocketAccept},
        {"server", ResponseHeaderType::kServer},
        {"server-timing", ResponseHeaderType::kServerTiming},
        {"set-cookie", ResponseHeaderType::kSetCookie},
        {"sourcemap", ResponseHeaderType::kSourceMap},
        {"strict-transport-security",
         ResponseHeaderType::kStrictTransportSecurity},
        {"timing-allow-origin", ResponseHeaderType::kTimingAllowOrigin},
        {"tk", ResponseHeaderType::kTk},
        {"trailer", ResponseHeaderType::kTrailer},
        {"transfer-encoding", ResponseHeaderType::kTransferEncoding},
        {"upgrade", ResponseHeaderType::kUpgrade},
        {"vary", ResponseHeaderType::kVary},
        {"via", ResponseHeaderType::kVia},
        {"warning", ResponseHeaderType::kWarning},
        {"www-authenticate", ResponseHeaderType::kWWWAuthenticate},
        {"x-content-type-options", ResponseHeaderType::kXContentTypeOptions},
        {"x-dns-prefetch-control", ResponseHeaderType::kXDNSPrefetchControl},
        {"x-frame-options", ResponseHeaderType::kXFrameOptions},
        {"x-xss-protection", ResponseHeaderType::kXXSSProtection},
    });

// All entries other than kOther and kNone are mapped.
static_assert(static_cast<size_t>(ResponseHeaderType::kMaxValue) - 1 ==
                  kResponseHeaderEntries.size(),
              "Invalid number of response header entries");

static_assert(ValidateHeaderEntries(kResponseHeaderEntries),
              "Invalid response header entries");

}  // namespace

IgnoredAction::IgnoredAction(extensions::ExtensionId extension_id,
                             url_request::IgnoredActionType action_type)
    : extension_id(std::move(extension_id)), action_type(action_type) {}

IgnoredAction::IgnoredAction(IgnoredAction&& rhs) = default;

BlockingResponse::BlockingResponse() = default;
BlockingResponse::BlockingResponse(BlockingResponse&& other) = default;
BlockingResponse& BlockingResponse ::operator=(BlockingResponse&& other) = default;
BlockingResponse::~BlockingResponse() = default;
BlockingResponse& BlockingResponse ::operator=(
    const BlockingResponse& other) = default;


bool BlockingResponse::operator==(const BlockingResponse& other) const {
  return std::tie(body, headers, status, status_text) ==
         std::tie(other.body, other.headers, other.status, other.status_text);
}

BlockingResponse BlockingResponse::Clone() const {
  BlockingResponse clone;
  clone.body = body;
  clone.headers = headers;
  clone.status = status;
  clone.status_text = status_text;
  return clone;
}

// TODO
bool BlockingResponse::empty() const {
  if (body.size() != 0) {
    return false;
  }
  if (headers.size() != 0) {
    return false;
  }
  if (status != "") {
    return false;
  }
  if (status_text != "") {
    return false;
  }

  return true;
}

EventResponseDelta::EventResponseDelta(
    const extensions::ExtensionId& extension_id,
    const base::Time& extension_install_time)
    : extension_id(extension_id),
      extension_install_time(extension_install_time),
      cancel(false),
      new_response({}) {}

EventResponseDelta::EventResponseDelta(EventResponseDelta&& other) = default;
EventResponseDelta& EventResponseDelta ::operator=(EventResponseDelta&& other) =
    default;

EventResponseDelta::~EventResponseDelta() = default;

bool InDecreasingExtensionInstallationTimeOrder(const EventResponseDelta& a,
                                                const EventResponseDelta& b) {
  return a.extension_install_time > b.extension_install_time;
}

base::Value::List StringToCharList(const std::string& s) {
  base::Value::List result;
  for (const auto& c : s)
    result.Append(*reinterpret_cast<const unsigned char*>(&c));
  return result;
}

bool CharListToString(const base::Value::List& list, std::string* out) {
  const size_t list_length = list.size();
  out->resize(list_length);
  int value = 0;
  for (size_t i = 0; i < list_length; ++i) {
    if (!list[i].is_int())
      return false;
    value = list[i].GetInt();
    if (value < 0 || value > 255)
      return false;
    unsigned char tmp = static_cast<unsigned char>(value);
    (*out)[i] = *reinterpret_cast<char*>(&tmp);
  }
  return true;
}

EventResponseDelta CalculateOnBeforeRequestDelta(
    const extensions::ExtensionId& extension_id,
    const base::Time& extension_install_time,
    bool cancel,
    BlockingResponse& response,
    const GURL& new_url) {
  EventResponseDelta result(extension_id, extension_install_time);
  result.cancel = cancel;
  result.new_response = std::move(response);
  result.new_url = new_url;
  return result;
}

void MergeCancelOfResponses(
    const EventResponseDeltas& deltas,
    std::optional<extensions::ExtensionId>* canceled_by_extension,
    std::optional<extensions::ExtensionId>* finished_by_extension) {
  *canceled_by_extension = std::nullopt;
  for (const auto& delta : deltas) {
    if (delta.cancel) {
      *canceled_by_extension = delta.extension_id;
      break;
    }
    if (!delta.new_response.empty()) {
      *finished_by_extension = delta.extension_id;
      break;
    }
  }
}

// Helper function for MergeRedirectUrlOfResponses() that allows ignoring
// all redirects but those to data:// urls and about:blank. This is important
// to treat these URLs as "cancel urls", i.e. URLs that extensions redirect
// to if they want to express that they want to cancel a request. This reduces
// the number of conflicts that we need to flag, as canceling is considered
// a higher precedence operation that redirects.
// Returns whether a redirect occurred.
static bool MergeRedirectUrlOfResponsesHelper(
    const GURL& url,
    const EventResponseDeltas& deltas,
    GURL* new_url,
    BlockingResponse* new_response,
    IgnoredActions* ignored_actions,
    bool consider_only_cancel_scheme_urls) {
  // Redirecting WebSocket handshake request is prohibited.
  if (url.SchemeIsWSOrWSS())
    return false;

  bool redirected = false;
  bool finished = false;

  for (const auto& delta : deltas) {
    if (!delta.new_response.empty()) {
      if (!finished || *new_response == delta.new_response) {
        *new_response = delta.new_response;
        finished = true;
      }
    }
    if (!delta.new_url.is_valid()) {
      continue;
    }
    if (consider_only_cancel_scheme_urls &&
        !delta.new_url.SchemeIs(url::kDataScheme) &&
        delta.new_url.spec() != "about:blank") {
      continue;
    }

    if (!redirected || *new_url == delta.new_url) {
      *new_url = delta.new_url;
      redirected = true;
    } else {
      ignored_actions->emplace_back(delta.extension_id,
                                    url_request::IgnoredActionType::kRedirect);
    }
  }
  return redirected;
}

void MergeRedirectUrlOfResponses(const GURL& url,
                                 const EventResponseDeltas& deltas,
                                 GURL* new_url,
                                 BlockingResponse* new_response,
                                 IgnoredActions* ignored_actions) {
  // First handle only redirects to data:// URLs and about:blank. These are a
  // special case as they represent a way of cancelling a request.
  if (MergeRedirectUrlOfResponsesHelper(url, deltas, new_url, new_response, ignored_actions,
                                        true)) {
    // If any extension cancelled a request by redirecting to a data:// URL or
    // about:blank, we don't consider the other redirects.
    return;
  }

  // Handle all other redirects.
  MergeRedirectUrlOfResponsesHelper(url, deltas, new_url, new_response, ignored_actions,
                                    false);
}

void MergeOnBeforeRequestResponses(const GURL& url,
                                   const EventResponseDeltas& deltas,
                                   GURL* new_url,
                                   BlockingResponse* new_response,
                                   IgnoredActions* ignored_actions) {
  MergeRedirectUrlOfResponses(url, deltas, new_url, new_response, ignored_actions);
}

void ClearCacheOnNavigation() {
  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    ClearCacheOnNavigationOnUI();
  } else {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&ClearCacheOnNavigationOnUI));
  }
}

// Converts the |name|, |value| pair of a http header to a HttpHeaders
// dictionary.
base::Value::Dict CreateHeaderDictionary(const std::string& name,
                                         const std::string& value) {
  base::Value::Dict header;
  header.Set(keys::kHeaderNameKey, name);
  if (base::IsStringUTF8(value)) {
    header.Set(keys::kHeaderValueKey, value);
  } else {
    header.Set(keys::kHeaderBinaryValueKey, StringToCharList(value));
  }
  return header;
}

}  // namespace extension_url_request_api_helpers
