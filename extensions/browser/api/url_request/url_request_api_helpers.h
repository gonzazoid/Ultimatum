// Copyright 2012 The Chromium Authors
// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Helper classes and functions used for the UrlRequest API.

#ifndef EXTENSIONS_BROWSER_API_URL_REQUEST_URL_REQUEST_API_HELPERS_H_
#define EXTENSIONS_BROWSER_API_URL_REQUEST_URL_REQUEST_API_HELPERS_H_

#include <list>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "extensions/common/api/url_request.h"
#include "extensions/common/extension_id.h"
#include "net/base/auth.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
struct UrlRequestInfo;

}  // namespace extensions

namespace extension_url_request_api_helpers {

using ResponseHeader = std::pair<std::string, std::string>;
using ResponseHeaders = std::vector<ResponseHeader>;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class RequestHeaderType {
  kNone = 0,
  kOther = 1,
  kAccept = 2,
  kAcceptCharset = 3,
  kAcceptEncoding = 4,
  kAcceptLanguage = 5,
  kAccessControlRequestHeaders = 6,
  kAccessControlRequestMethod = 7,
  kAuthorization = 8,
  kCacheControl = 9,
  kConnection = 10,
  kContentEncoding = 11,
  kContentLanguage = 12,
  kContentLength = 13,
  kContentLocation = 14,
  kContentType = 15,
  kCookie = 16,
  kDate = 17,
  kDnt = 18,
  kEarlyData = 19,
  kExpect = 20,
  kForwarded = 21,
  kFrom = 22,
  kHost = 23,
  kIfMatch = 24,
  kIfModifiedSince = 25,
  kIfNoneMatch = 26,
  kIfRange = 27,
  kIfUnmodifiedSince = 28,
  kKeepAlive = 29,
  kOrigin = 30,
  kPragma = 31,
  kProxyAuthorization = 32,
  kProxyConnection = 33,
  kRange = 34,
  kReferer = 35,
  //  kSecOriginPolicy = 36, // no longer shipping
  kTe = 37,
  kTransferEncoding = 38,
  kUpgrade = 39,
  kUpgradeInsecureRequests = 40,
  kUserAgent = 41,
  kVia = 42,
  kWarning = 43,
  kXForwardedFor = 44,
  kXForwardedHost = 45,
  kXForwardedProto = 46,
  kMaxValue = kXForwardedProto,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ResponseHeaderType {
  kNone = 0,
  kOther = 1,
  kAcceptPatch = 2,
  kAcceptRanges = 3,
  kAccessControlAllowCredentials = 4,
  kAccessControlAllowHeaders = 5,
  kAccessControlAllowMethods = 6,
  kAccessControlAllowOrigin = 7,
  kAccessControlExposeHeaders = 8,
  kAccessControlMaxAge = 9,
  kAge = 10,
  kAllow = 11,
  kAltSvc = 12,
  kCacheControl = 13,
  kClearSiteData = 14,
  kConnection = 15,
  kContentDisposition = 16,
  kContentEncoding = 17,
  kContentLanguage = 18,
  kContentLength = 19,
  kContentLocation = 20,
  kContentRange = 21,
  kContentSecurityPolicy = 22,
  kContentSecurityPolicyReportOnly = 23,
  kContentType = 24,
  kDate = 25,
  kETag = 26,
  kExpectCT = 27,
  kExpires = 28,
  kFeaturePolicy = 29,
  kKeepAlive = 30,
  kLargeAllocation = 31,
  kLastModified = 32,
  kLocation = 33,
  kPragma = 34,
  kProxyAuthenticate = 35,
  kProxyConnection = 36,
  kPublicKeyPins = 37,
  kPublicKeyPinsReportOnly = 38,
  kReferrerPolicy = 39,
  kRefresh = 40,
  kRetryAfter = 41,
  kSecWebSocketAccept = 42,
  kServer = 43,
  kServerTiming = 44,
  kSetCookie = 45,
  kSourceMap = 46,
  kStrictTransportSecurity = 47,
  kTimingAllowOrigin = 48,
  kTk = 49,
  kTrailer = 50,
  kTransferEncoding = 51,
  kUpgrade = 52,
  kVary = 53,
  kVia = 54,
  kWarning = 55,
  kWWWAuthenticate = 56,
  kXContentTypeOptions = 57,
  kXDNSPrefetchControl = 58,
  kXFrameOptions = 59,
  kXXSSProtection = 60,
  kMaxValue = kXXSSProtection
};

struct IgnoredAction {
  IgnoredAction(extensions::ExtensionId extension_id,
                extensions::api::url_request::IgnoredActionType action_type);
  IgnoredAction(const IgnoredAction&) = delete;
  IgnoredAction(IgnoredAction&& rhs);
  IgnoredAction& operator=(const IgnoredAction&) = delete;

  extensions::ExtensionId extension_id;
  extensions::api::url_request::IgnoredActionType action_type;
};

using IgnoredActions = std::vector<IgnoredAction>;

struct BlockingResponse {
  BlockingResponse();
  BlockingResponse(const BlockingResponse&) = delete;
  BlockingResponse(BlockingResponse&& other);
  BlockingResponse& operator=(const BlockingResponse&);
  BlockingResponse& operator=(BlockingResponse&& other);
  ~BlockingResponse();

  bool operator==(const BlockingResponse& other) const;

  BlockingResponse Clone() const;
  bool empty() const;

  base::Value::BlobStorage body;
  ResponseHeaders headers;
  std::string status;
  std::string status_text;
};

// Contains the modification an extension wants to perform on an event.
struct EventResponseDelta {
  EventResponseDelta(const extensions::ExtensionId& extension_id,
                     const base::Time& extension_install_time);
  EventResponseDelta(const EventResponseDelta&) = delete;
  EventResponseDelta(EventResponseDelta&& other);
  EventResponseDelta& operator=(const EventResponseDelta&) = delete;
  EventResponseDelta& operator=(EventResponseDelta&& other);
  ~EventResponseDelta();

  // ID of the extension that sent this response.
  extensions::ExtensionId extension_id;

  // The time that the extension was installed. Used for deciding order of
  // precedence in case multiple extensions respond with conflicting
  // decisions.
  base::Time extension_install_time;

  // Response values. These are mutually exclusive.
  bool cancel;
  GURL new_url;
  BlockingResponse new_response;

  // Newly introduced or overridden request headers.
  net::HttpRequestHeaders modified_request_headers;

  // Keys of request headers to be deleted.
  std::vector<std::string> deleted_request_headers;

  // Messages that shall be sent to the background/event/... pages of the
  // extension.
  std::set<std::string> messages_to_extension;
};

using EventResponseDeltas = std::list<EventResponseDelta>;

// Comparison operator that returns true if the extension that caused
// |a| was installed after the extension that caused |b|.
bool InDecreasingExtensionInstallationTimeOrder(const EventResponseDelta& a,
                                                const EventResponseDelta& b);

// Converts a string to a list of integers, each in 0..255.
base::Value::List StringToCharList(const std::string& s);

// Converts a list of integer values between 0 and 255 into a string |*out|.
// Returns true if the conversion was successful.
bool CharListToString(const base::Value::List& list, std::string* out);

// The following functions calculate and return the modifications to requests
// commanded by extension handlers. All functions take the id of the extension
// that commanded a modification, the installation time of this extension (used
// for defining a precedence in conflicting modifications) and whether the
// extension requested to |cancel| the request. Other parameters depend on a
// the signal handler.

EventResponseDelta CalculateOnBeforeRequestDelta(
    const extensions::ExtensionId& extension_id,
    const base::Time& extension_install_time,
    bool cancel,
    BlockingResponse& response,
    const GURL& new_url);

// These functions merge the responses (the |deltas|) of request handlers.
// The |deltas| need to be sorted in decreasing order of precedence of
// extensions. In case extensions had |deltas| that could not be honored, their
// IDs are reported in |conflicting_extensions|.

// Stores in |*canceled_by_extension| whether any extension wanted to cancel the
// request, std::nullopt if none did, the extension id otherwise.
void MergeCancelOfResponses(
    const EventResponseDeltas& deltas,
    std::optional<extensions::ExtensionId>* canceled_by_extension,
    std::optional<extensions::ExtensionId>* finished_by_extension);
// Stores in |*new_url| the redirect request of the extension with highest
// precedence. Extensions that did not command to redirect the request are
// ignored in this logic.
void MergeRedirectUrlOfResponses(const GURL& url,
                                 const EventResponseDeltas& deltas,
                                 GURL* new_url,
                                 IgnoredActions* ignored_actions);
// Stores in |*new_url| the redirect request of the extension with highest
// precedence. Extensions that did not command to redirect the request are
// ignored in this logic.
void MergeOnBeforeRequestResponses(const GURL& url,
                                   const EventResponseDeltas& deltas,
                                   GURL* new_url,
                                   BlockingResponse* new_response,
                                   IgnoredActions* ignored_actions);

// Triggers clearing any back-forward caches and each renderer's in-memory cache
// the next time it navigates.
void ClearCacheOnNavigation();

// Converts the |name|, |value| pair of a http header to a HttpHeaders
// dictionary.
base::Value::Dict CreateHeaderDictionary(const std::string& name,
                                         const std::string& value);


}  // namespace extension_url_request_api_helpers

#endif  // EXTENSIONS_BROWSER_API_URL_REQUEST_URL_REQUEST_API_HELPERS_H_
