// Copyright 2023 The Chromium Authors
// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/url_request/extension_url_request_event_router.h"

#include <algorithm>
#include <string_view>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/no_destructor.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/url_request/url_request_permission_helper.h"
#include "extensions/browser/api/url_request/url_request_api_constants.h"
#include "extensions/browser/api/url_request/url_request_api_helpers.h"
#include "extensions/browser/api/url_request/url_request_event_details.h"
#include "extensions/browser/api/url_request/url_request_event_router_factory.h"
#include "extensions/browser/api/url_request/url_request_info.h"
#include "extensions/browser/api/url_request/url_request_permissions.h"
#include "extensions/browser/api/url_request/url_request_time_tracker.h"
#include "extensions/browser/api_activity_monitor.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_map.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/browser/guest_view/guest_view_events.h"
#endif

using content::BrowserThread;

namespace extensions {

namespace {

namespace helpers = extension_url_request_api_helpers;
namespace keys = extension_url_request_api_constants;
namespace url_request = api::url_request;

// Describes the action taken by the Web Request API for a given stage of a web
// request.
// These values are written to logs.  New enum values can be added, but existing
// enum values must never be renumbered or deleted and reused.
enum RequestAction {
  CANCEL = 0,
  REDIRECT = 1,
  MODIFY_REQUEST_HEADERS = 2,
  MODIFY_RESPONSE_HEADERS = 3,
  SET_AUTH_CREDENTIALS = 4,
  MAX
};

constexpr char kEventMessage[] = "webViewInternal.onMessage";

constexpr char kUrlRequestEventPrefix[] = "urlRequest.";
constexpr char kWebViewEventPrefix[] = "webViewInternal.";

constexpr size_t kUrlRequestEventPrefixLen =
    std::char_traits<char>::length(kUrlRequestEventPrefix);
constexpr size_t kWebViewEventPrefixLen =
    std::char_traits<char>::length(kWebViewEventPrefix);

// List of all the urlRequest events. Note: this doesn't include
// "onActionIgnored" which is not related to a request's lifecycle and is
// handled as a normal event (as opposed to a UrlRequestEvent at the bindings
// layer).
constexpr const char* kUrlRequestEvents[] = {
    url_request::OnBeforeRequest::kEventName,
};

events::HistogramValue GetEventHistogramValue(const std::string& event_name) {
  // Event names will either be urlRequest events, or guest view (probably web
  // view) events that map to urlRequest events. Check urlRequest first.
  static constexpr struct ValueAndName {
    events::HistogramValue histogram_value;
    const char* const event_name;
  } values_and_names[] = {
      {events::URL_REQUEST_ON_BEFORE_REQUEST,
       url_request::OnBeforeRequest::kEventName},
   };
  static_assert(std::size(kUrlRequestEvents) == std::size(values_and_names),
                "kUrlRequestEvents and values_and_names must be the same");
  for (const ValueAndName& value_and_name : values_and_names) {
    if (value_and_name.event_name == event_name) {
      return value_and_name.histogram_value;
    }
  }

#if BUILDFLAG(ENABLE_GUEST_VIEW)
  // If there is no urlRequest event, it might be a guest view urlRequest event.
  events::HistogramValue guest_view_histogram_value =
      guest_view_events::GetEventHistogramValue(event_name);
  if (guest_view_histogram_value != events::UNKNOWN) {
    return guest_view_histogram_value;
  }
#endif

  // There is no histogram value for this event name. It should be added to
  // either the mapping here, or in guest_view_events.
  NOTREACHED_IN_MIGRATION()
      << "Event " << event_name << " must have a histogram value";
  return events::UNKNOWN;
}

const char* GetRequestStageAsString(UrlRequestEventRouter::EventTypes type) {
  switch (type) {
    case UrlRequestEventRouter::kInvalidEvent:
      return "Invalid";
    case UrlRequestEventRouter::kOnBeforeRequest:
      return keys::kOnBeforeRequest;
  }
  NOTREACHED_IN_MIGRATION();
  return "Not reached";
}

void LogRequestAction(RequestAction action) {
  DCHECK_NE(RequestAction::MAX, action);
  UMA_HISTOGRAM_ENUMERATION("Extensions.UrlRequestAction", action,
                            RequestAction::MAX);
  TRACE_EVENT1("extensions", "UrlRequestAction", "action", action);
}

// Returns the corresponding EventTypes for the given |event_name|. If
// |event_name| is an invalid event, returns EventTypes::kInvalidEvent.
UrlRequestEventRouter::EventTypes GetEventTypeFromEventName(
    std::string_view event_name) {
  constexpr auto kRequestStageMap = base::MakeFixedFlatMap<
      std::string_view, UrlRequestEventRouter::EventTypes>(
      {
        {keys::kOnBeforeRequest, UrlRequestEventRouter::kOnBeforeRequest}
      });
  static_assert(kRequestStageMap.size() == std::size(kUrlRequestEvents));

  // Canonicalize the |event_name| to the request stage.
  if (base::StartsWith(event_name, kUrlRequestEventPrefix)) {
    event_name.remove_prefix(kUrlRequestEventPrefixLen);
  } else if (base::StartsWith(event_name, kWebViewEventPrefix)) {
    event_name.remove_prefix(kWebViewEventPrefixLen);
  } else {
    return UrlRequestEventRouter::kInvalidEvent;
  }
  const auto it = kRequestStageMap.find(event_name);
  return it == kRequestStageMap.end() ? UrlRequestEventRouter::kInvalidEvent
                                      : it->second;
}

bool IsUrlRequestEvent(std::string_view event_name) {
  return GetEventTypeFromEventName(event_name) !=
         UrlRequestEventRouter::kInvalidEvent;
}

// Returns whether |request| has been triggered by an extension enabled in
// |context|.
bool IsRequestFromExtension(const UrlRequestInfo& request,
                            content::BrowserContext* context) {
  if (request.render_process_id == -1) {
    return false;
  }

  const Extension* extension =
      ProcessMap::Get(context)->GetEnabledExtensionByProcessID(
          request.render_process_id);
  return extension && !extension->is_hosted_app();
}

// Sends an event to subscribers of chrome.declarativeWebRequest.onMessage or
// to subscribers of webview.onMessage if the action is being operated upon
// a <webview> guest renderer.
// |extension_id| identifies the extension that sends and receives the event.
// |is_web_view_guest| indicates whether the action is for a <webview>.
// |web_view_instance_id| is a valid if |is_web_view_guest| is true.
// |event_details| is passed to the event listener.
void SendOnMessageEventOnUI(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id,
    bool is_web_view_guest,
    int web_view_instance_id,
    std::unique_ptr<UrlRequestEventDetails> event_details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!ExtensionsBrowserClient::Get()->IsValidContext(browser_context)) {
    return;
  }

  base::Value::List event_args;
  event_args.Append(event_details->GetAndClearDict());

  EventRouter* event_router = EventRouter::Get(browser_context);

  mojom::EventFilteringInfoPtr event_filtering_info =
      mojom::EventFilteringInfo::New();

  events::HistogramValue histogram_value = events::UNKNOWN;
  std::string event_name;
  // The instance ID uniquely identifies a <webview> instance within an embedder
  // process. We use a filter here so that only event listeners for a particular
  // <webview> will fire.
  if (is_web_view_guest) {
    event_filtering_info->has_instance_id = true;
    event_filtering_info->instance_id = web_view_instance_id;
    histogram_value = events::WEB_VIEW_INTERNAL_ON_MESSAGE;
    event_name = kEventMessage;
  }

  auto event = std::make_unique<Event>(
      histogram_value, event_name, std::move(event_args), browser_context,
      /*restrict_to_context_type=*/std::nullopt, GURL(),
      EventRouter::USER_GESTURE_UNKNOWN, std::move(event_filtering_info));
  event_router->DispatchEventToExtension(extension_id, std::move(event));
}

// We hide events from the system context as well as sensitive requests.
bool ShouldHideEvent(content::BrowserContext* browser_context,
                     const UrlRequestInfo& request) {
  return !browser_context ||
         UrlRequestPermissions::HideRequest(
             UrlRequestPermissionHelper::Get(browser_context), request);
}

// Returns event details for a given request.
std::unique_ptr<UrlRequestEventDetails> CreateEventDetails(
    const UrlRequestInfo& request,
    int extra_info_spec) {
  return std::make_unique<UrlRequestEventDetails>(request, extra_info_spec);
}

using CallbacksForPageLoad = std::list<base::OnceClosure>;

// TODO(crbug.com/40264286): We need to investigate why this is a global
// structure instead of a per-BrowserContext structure. It seems incorrect
// that a page load in one BrowserContext should interact with a page load
// in another one.
CallbacksForPageLoad& GetCallbacksForPageLoad() {
  static base::NoDestructor<CallbacksForPageLoad> instance;
  return *instance.get();
}

ExtensionUrlRequestTimeTracker& GetExtensionUrlRequestTimeTracker() {
  static base::NoDestructor<ExtensionUrlRequestTimeTracker> instance;
  return *instance.get();
}

class CrossContextData {
 public:
  CrossContextData() = default;
  ~CrossContextData() = default;
  CrossContextData(const CrossContextData&) = delete;
  CrossContextData& operator=(const CrossContextData&) = delete;

  static CrossContextData& Get() {
    static base::NoDestructor<CrossContextData> instance;
    return *instance.get();
  }

  content::BrowserContext* GetCrossBrowserContext(
      content::BrowserContext* browser_context) {
    const auto it = cross_context_data_.find(browser_context);
    return it == cross_context_data_.end() ? nullptr : it->second;
  }

  void AddContext(content::BrowserContext* original_browser_context,
                  content::BrowserContext* otr_browser_context) {
    cross_context_data_[original_browser_context] = otr_browser_context;
    cross_context_data_[otr_browser_context] = original_browser_context;
  }

  void RemoveContext(content::BrowserContext* browser_context) {
    // This context can be either the original one, or the OTR one. Either
    // way, we need to remove both entries.
    auto it = cross_context_data_.find(browser_context);
    if (it != cross_context_data_.end()) {
      cross_context_data_.erase(it->second);
      cross_context_data_.erase(it);
    }
  }

 private:
  using CrossContextMap =
      std::map<content::BrowserContext*, content::BrowserContext*>;

  // For each each on-the-record context that has an off-the-record context,
  // this bi-map contains an entry for both contexts where the value is the
  // other context.
  CrossContextMap cross_context_data_;
};

void ClearCrossContextData(content::BrowserContext* browser_context) {
  CrossContextData::Get().RemoveContext(browser_context);
}

}  // namespace

UrlRequestEventRouter::UrlRequestEventRouter(content::BrowserContext* context)
    : browser_context_(context) {}

UrlRequestEventRouter::~UrlRequestEventRouter() = default;

void UrlRequestEventRouter::Shutdown() {
  // TODO(crbug.com/40264286): This overlaps with OnOTRBrowserContextDestroyed.
  // We should decide whether this can be cleaned up.
  OnBrowserContextShutdown(browser_context_);
  ClearCrossContextData(browser_context_);
}

// static
UrlRequestEventRouter* UrlRequestEventRouter::Get(
    content::BrowserContext* browser_context) {
  return UrlRequestEventRouterFactory::GetForBrowserContext(browser_context);
}

// static
std::vector<std::string> UrlRequestEventRouter::GetEventNames() {
  std::vector<std::string> result;
  result.reserve(std::size(kUrlRequestEvents) * 2);
  for (std::string event_name : kUrlRequestEvents) {
    // The urlRequest event.
    result.push_back(event_name);

    // The corresponding webview event name.
    event_name.replace(0, kUrlRequestEventPrefixLen, kWebViewEventPrefix);
    result.push_back(event_name);
  }

  return result;
}

// Represents a single unique listener to an event, along with whatever filter
// parameters and extra_info_spec were specified at the time the listener was
// added.
// NOTE(benjhayden) New APIs should not use this sub_event_name trick! It does
// not play well with event pages. See downloads.onDeterminingFilename and
// ExtensionDownloadsEventRouter for an alternative approach.
UrlRequestEventRouter::EventListener::EventListener(ID id)
    : id(std::move(id)) {}
UrlRequestEventRouter::EventListener::~EventListener() = default;

// Contains info about requests that are blocked waiting for a response from
// an extension.
struct UrlRequestEventRouter::BlockedRequest {
  BlockedRequest() = default;

  // Information about the request that is being blocked. Not owned.
  raw_ptr<const UrlRequestInfo> request = nullptr;

  // Whether the request originates from an incognito tab.
  bool is_incognito = false;

  // The event that we're currently blocked on.
  EventTypes event = kInvalidEvent;

  // The number of event handlers that we are awaiting a response from.
  int num_handlers_blocking = 0;

  // The callback to call when we get a response from all event handlers.
  net::CompletionOnceCallback callback;

  // If non-empty, this contains the new URL that the request will redirect to.
  // Only valid for OnBeforeRequest and OnHeadersReceived.
  raw_ptr<GURL> new_url = nullptr;

  raw_ptr<extension_url_request_api_helpers::BlockingResponse> response = nullptr;

  // Time the request was paused. Used for logging purposes.
  base::Time blocking_time;

  // Changes requested by extensions.
  helpers::EventResponseDeltas response_deltas;
};

namespace {

helpers::EventResponseDelta CalculateDelta(
    content::BrowserContext* browser_context,
    UrlRequestEventRouter::BlockedRequest* blocked_request,
    UrlRequestEventRouter::EventResponse* response,
    int extra_info_spec) {
  switch (blocked_request->event) {
    case UrlRequestEventRouter::kOnBeforeRequest:
      return helpers::CalculateOnBeforeRequestDelta(
          response->extension_id, response->extension_install_time,
          response->cancel, response->response, response->new_url);
    default:
      NOTREACHED_IN_MIGRATION();
      return helpers::EventResponseDelta("", base::Time());
  }
}

}  // namespace

bool UrlRequestEventRouter::RequestFilter::InitFromValue(
    const base::Value::Dict& value,
    std::string* error) {
  if (!value.Find("urls")) {
    return false;
  }

  for (const auto dict_item : value) {
    if (dict_item.first == "urls" && dict_item.second.is_list()) {
      for (const auto& item : dict_item.second.GetList()) {
        std::string url;
        URLPattern pattern(URLPattern::SCHEME_HTTP | URLPattern::SCHEME_HTTPS |
                           URLPattern::SCHEME_FTP | URLPattern::SCHEME_FILE |
                           URLPattern::SCHEME_EXTENSION |
                           URLPattern::SCHEME_WS | URLPattern::SCHEME_WSS |
                           URLPattern::SCHEME_UUID_IN_PACKAGE);
        if (item.is_string()) {
          url = item.GetString();
        }

        // Parse will fail on an empty url, so we don't need to distinguish
        // between `item` not being a string and `item` being an empty string.
        if (url.empty() ||
            pattern.Parse(url) != URLPattern::ParseResult::kSuccess) {
          *error = ErrorUtils::FormatErrorMessage(
              keys::kInvalidRequestFilterUrl, url);
          return false;
        }
        urls.AddPattern(pattern);
      }
    } else if (dict_item.first == "types" && dict_item.second.is_list()) {
      for (const auto& type : dict_item.second.GetList()) {
        std::string type_str;
        if (type.is_string()) {
          type_str = type.GetString();
        }
        types.push_back(UrlRequestResourceType::OTHER);
        if (type_str.empty() ||
            !ParseUrlRequestResourceType(type_str, &types.back())) {
          return false;
        }
      }
    } else if (dict_item.first == "tabId" && dict_item.second.is_int()) {
      tab_id = dict_item.second.GetInt();
    } else if (dict_item.first == "windowId" && dict_item.second.is_int()) {
      window_id = dict_item.second.GetInt();
    } else {
      return false;
    }
  }
  return true;
}

UrlRequestEventRouter::EventResponse::EventResponse(
    const ExtensionId& extension_id,
    const base::Time& extension_install_time)
    : extension_id(extension_id),
      extension_install_time(extension_install_time),
      cancel(false) {}

UrlRequestEventRouter::EventResponse::~EventResponse() = default;

UrlRequestEventRouter::RequestFilter::RequestFilter()
    : tab_id(-1), window_id(-1) {}
UrlRequestEventRouter::RequestFilter::~RequestFilter() = default;

UrlRequestEventRouter::RequestFilter::RequestFilter(RequestFilter&& other) =
    default;
UrlRequestEventRouter::RequestFilter&
UrlRequestEventRouter::RequestFilter::operator=(RequestFilter&& other) =
    default;

UrlRequestEventRouter::SignaledRequestIDTracker::SignaledRequestIDTracker() =
    default;
UrlRequestEventRouter::SignaledRequestIDTracker::~SignaledRequestIDTracker() =
    default;
UrlRequestEventRouter::SignaledRequestIDTracker::SignaledRequestIDTracker(
    SignaledRequestIDTracker&&) = default;

bool UrlRequestEventRouter::SignaledRequestIDTracker::GetAndSet(
    uint64_t request_id,
    EventTypes event_type) {
  auto iter = signaled_requests_.find(request_id);
  if (iter == signaled_requests_.end()) {
    signaled_requests_[request_id] = event_type;
    return false;
  }
  bool was_signaled_before = iter->second & event_type;
  iter->second |= event_type;
  return was_signaled_before;
}

void UrlRequestEventRouter::SignaledRequestIDTracker::ClearEventType(
    uint64_t request_id,
    EventTypes event_type) {
  auto iter = signaled_requests_.find(request_id);
  if (iter != signaled_requests_.end()) {
    iter->second &= ~event_type;
  }
}

UrlRequestEventRouter::BrowserContextData::BrowserContextData() = default;
UrlRequestEventRouter::BrowserContextData::BrowserContextData(
    BrowserContextData&&) = default;
UrlRequestEventRouter::BrowserContextData::~BrowserContextData() = default;

UrlRequestEventRouter::EventListener::ID::ID(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id,
    const std::string& sub_event_name,
    int render_process_id,
    int web_view_instance_id,
    int worker_thread_id,
    int64_t service_worker_version_id)
    : browser_context(browser_context),
      extension_id(extension_id),
      sub_event_name(sub_event_name),
      render_process_id(render_process_id),
      web_view_instance_id(web_view_instance_id),
      worker_thread_id(worker_thread_id),
      service_worker_version_id(service_worker_version_id) {}

UrlRequestEventRouter::EventListener::ID::ID(const ID& source) = default;
UrlRequestEventRouter::EventListener::ID::ID(ID&& source) = default;

bool UrlRequestEventRouter::EventListener::ID::operator==(
    const ID& that) const {
  // Since EventListeners are segmented by browser_context, check that
  // last, as it is exceedingly unlikely to be different.
  return extension_id == that.extension_id &&
         sub_event_name == that.sub_event_name &&
         web_view_instance_id == that.web_view_instance_id &&
         render_process_id == that.render_process_id &&
         worker_thread_id == that.worker_thread_id &&
         service_worker_version_id == that.service_worker_version_id &&
         browser_context == that.browser_context;
}

//
//  UrlRequestEventRouter
//

int UrlRequestEventRouter::OnBeforeRequest(
    content::BrowserContext* browser_context,
    UrlRequestInfo* request,
    net::CompletionOnceCallback callback,
    GURL* new_url,
    extension_url_request_api_helpers::BlockingResponse* response,
    bool* should_collapse_initiator) {
  DCHECK(should_collapse_initiator);

  if (ShouldHideEvent(browser_context, *request)) {
    return net::OK;
  }

  if (IsPageLoad(*request)) {
    NotifyPageLoad();
  }

  bool has_listener = false;
  for (const auto& kv :
       data_[GetBrowserContextID(browser_context)].active_listeners) {
    if (!kv.second.empty()) {
      has_listener = true;
      break;
    }
  }
  GetExtensionUrlRequestTimeTracker().LogRequestStartTime(
      request->id, base::TimeTicks::Now(), has_listener);

  const bool is_incognito_context = browser_context->IsOffTheRecord();

  // CRX requests information can be intercepted here.
  // May be null for browser-initiated requests such as navigations.
  if (request->initiator) {
    const std::string& scheme = request->initiator->scheme();
    const ExtensionId& extension_id = request->initiator->host();
    const GURL& request_url = request->url;
    if (scheme == extensions::kExtensionScheme) {
      ExtensionsBrowserClient::Get()->NotifyExtensionRemoteHostContacted(
          browser_context, extension_id, request_url);
    }
  }

  // Whether to initialized `blocked_requests_`.
  bool initialize_blocked_requests = false;

  int extra_info_spec = 0;
  RawListeners listeners = GetMatchingListeners(
      browser_context, url_request::OnBeforeRequest::kEventName, request,
      &extra_info_spec);
  if (!listeners.empty() &&
      !GetAndSetSignaled(browser_context, request->id, kOnBeforeRequest)) {
    std::unique_ptr<UrlRequestEventDetails> event_details(
        CreateEventDetails(*request, extra_info_spec));
    event_details->SetRequestBody(request);

    GetExtensionUrlRequestTimeTracker().LogBeforeRequestDispatchTime(
        request->id, base::TimeTicks::Now());

    initialize_blocked_requests |= DispatchEvent(
        browser_context, request, listeners, std::move(event_details));
  }

  if (!initialize_blocked_requests) {
    return net::OK;  // Nobody saw a reason for modifying the request.
  }

  BlockedRequest& blocked_request =
      GetOrAddBlockedRequest(browser_context, request->id);
  blocked_request.event = kOnBeforeRequest;
  blocked_request.is_incognito |= is_incognito_context;
  blocked_request.request = request;
  blocked_request.callback = std::move(callback);
  blocked_request.new_url = new_url;
  blocked_request.response = response;

  if (blocked_request.num_handlers_blocking == 0) {
    // If there are no blocking handlers, only the declarative rules tried
    // to modify the request and we can respond synchronously.
    return ExecuteDeltas(browser_context, request, false /* call_callback*/);
  }
  return net::ERR_IO_PENDING;
}

int UrlRequestEventRouter::OnHeadersReceived(
    content::BrowserContext* browser_context,
    UrlRequestInfo* request,
    net::CompletionOnceCallback callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* preserve_fragment_on_redirect_url,
    bool* should_collapse_initiator) {
  CHECK(should_collapse_initiator);

  return net::OK;  // Nobody saw a reason for modifying the request.

}

void UrlRequestEventRouter::OnBeforeRedirect(
    content::BrowserContext* browser_context,
    const UrlRequestInfo* request,
    const GURL& new_location) {

  return;

}

void UrlRequestEventRouter::OnRequestWillBeDestroyed(
    content::BrowserContext* browser_context,
    const UrlRequestInfo* request) {
  ClearPendingCallbacks(browser_context, *request);
  GetSignaledRequestIDTracker(browser_context).ClearRequest(request->id);
  GetExtensionUrlRequestTimeTracker().LogRequestEndTime(request->id,
                                                        base::TimeTicks::Now());
}

void UrlRequestEventRouter::ClearPendingCallbacks(
    content::BrowserContext* browser_context,
    const UrlRequestInfo& request) {
  ClearBlockedRequest(browser_context, request.id);
}

bool UrlRequestEventRouter::DispatchEvent(
    content::BrowserContext* browser_context,
    const UrlRequestInfo* request,
    const RawListeners& listeners,
    std::unique_ptr<UrlRequestEventDetails> event_details) {
  // TODO(mpcomplete): Consider consolidating common (extension_id,json_args)
  // pairs into a single message sent to a list of sub_event_names.
  int num_handlers_blocking = 0;

  std::unique_ptr<ListenerIDs> listeners_to_dispatch(new ListenerIDs);
  listeners_to_dispatch->reserve(listeners.size());
  for (EventListener* listener : listeners) {
    listeners_to_dispatch->push_back(listener->id);
    // if (listener->IsBlocking()) {
    listener->blocked_requests.insert(request->id);
    ++num_handlers_blocking;
    // }
  }

  DispatchEventToListeners(browser_context, std::move(listeners_to_dispatch),
                           request->id, std::move(event_details));

  if (num_handlers_blocking > 0) {
    BlockedRequest& blocked_request =
        GetOrAddBlockedRequest(browser_context, request->id);
    blocked_request.request = request;
    blocked_request.is_incognito |= browser_context->IsOffTheRecord();
    blocked_request.num_handlers_blocking += num_handlers_blocking;
    blocked_request.blocking_time = base::Time::Now();
    return true;
  }

  return false;
}

void UrlRequestEventRouter::DispatchEventToListeners(
    content::BrowserContext* browser_context,
    std::unique_ptr<ListenerIDs> listener_ids,
    uint64_t request_id,
    std::unique_ptr<UrlRequestEventDetails> event_details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!listener_ids->empty());
  DCHECK(event_details.get());

  std::string event_name =
      EventRouter::GetBaseEventName((*listener_ids)[0].sub_event_name);
  DCHECK(IsUrlRequestEvent(event_name));

  BrowserContextData& data = data_[GetBrowserContextID(browser_context)];

  // Gather all potential sources for listeners. They may be in:
  // - The active listeners for this context.
  // - The inactive listeners for this context.
  // - The active listeners for the cross-browser context.
  // - The inactive listeners for the cross-browser context.
  Listeners& active_listeners = data.active_listeners[event_name];
  Listeners& inactive_listeners = data.inactive_listeners[event_name];
  Listeners* cross_active_listeners = nullptr;
  Listeners* cross_inactive_listeners = nullptr;
  content::BrowserContext* const cross_context =
      GetCrossBrowserContext(browser_context);
  if (cross_context) {
    auto& cross_data = data_[GetBrowserContextID(cross_context)];
    cross_active_listeners = &cross_data.active_listeners[event_name];
    cross_inactive_listeners = &cross_data.inactive_listeners[event_name];
  }

  for (const EventListener::ID& id : *listener_ids) {
    // Look for the event listener in the different listener sources.
    bool is_active = id.render_process_id != -1;
    Listeners* on_the_record_listeners =
        is_active ? &active_listeners : &inactive_listeners;
    Listeners* cross_listeners =
        is_active ? cross_active_listeners : cross_inactive_listeners;

    bool crosses_incognito = false;
    const EventListener* listener =
        FindEventListenerInContainer(id, *on_the_record_listeners);
    if (!listener && cross_listeners) {
      listener = FindEventListenerInContainer(id, *cross_listeners);
      crosses_incognito = true;
    }

    // It's possible the listener was removed. If so, bail.
    if (!listener) {
      continue;
    }

    DCHECK(listener->id == id);

    // Check that the listener's process is also still valid. This only applies
    // for active listeners.
    content::RenderProcessHost* render_process = nullptr;
    if (is_active) {
      render_process = content::RenderProcessHost::FromID(id.render_process_id);
      if (!render_process) {
        continue;  // The process for an active listener shut down. Bail.
      }
    }

    // Filter out the optional keys that this listener didn't request.
    base::Value::List args_filtered;

    args_filtered.Append(event_details->GetFilteredDict(
        listener->extra_info_spec, UrlRequestPermissionHelper::Get(browser_context),
        listener->id.extension_id, crosses_incognito));
    if (is_active) {
      DCHECK(render_process);
      // TODO(devlin): Upgrade this to a CHECK().
      if (ExtensionsBrowserClient::Get()->IsValidContext(browser_context)) {
        // Active listeners use a bespoke dispatching mechanism.
        // TODO(devlin): Now that the webRequest API is entirely handled on the
        // UI thread (it used to be on the IO thread), can we just use the
        // regular event dispatching code for this case, as well?
        // TODO(gonzazoid) keep eye on it
        EventRouter::Get(id.browser_context)
            ->DispatchEventToSender(
                render_process, id.browser_context,
                /*host_id=*/
                mojom::HostID(mojom::HostID::HostType::kExtensions,
                              listener->id.extension_id),
                listener->histogram_value, listener->id.sub_event_name,
                listener->id.worker_thread_id,
                listener->id.service_worker_version_id,
                std::move(args_filtered), mojom::EventFilteringInfo::New());
      }
    } else {
      DCHECK_EQ(-1, id.service_worker_version_id);
      // In the event of a lazy listener, we go through normal extension
      // event dispatching code, which is responsible for waking up the
      // lazy context.
      std::unique_ptr<Event> event =
          std::make_unique<Event>(listener->histogram_value, id.sub_event_name,
                                  std::move(args_filtered));
      // Add a callback to the event in case we find we cannot dispatch to the
      // extension listener (as can happen if the extension fails to re-register
      // the event listener synchronously). If this happens, we treat the event
      // as handled so as to not block indefinitely.
      event->cannot_dispatch_callback = base::BindRepeating(
          &UrlRequestEventRouter::OnEventHandled,
          weak_ptr_factory_.GetWeakPtr(), id.browser_context, id.extension_id,
          event_name, id.sub_event_name, request_id, id.render_process_id,
          id.web_view_instance_id, id.worker_thread_id,
          id.service_worker_version_id, nullptr);
      EventRouter::Get(id.browser_context)
          ->DispatchEventToExtension(id.extension_id, std::move(event));
    }
  }
}

void UrlRequestEventRouter::OnEventHandled(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id,
    const std::string& event_name,
    const std::string& sub_event_name,
    uint64_t request_id,
    int render_process_id,
    int web_view_instance_id,
    int worker_thread_id,
    int64_t service_worker_version_id,
    std::unique_ptr<EventResponse> response) {
  BrowserContextData& context_data =
      data_[GetBrowserContextID(browser_context)];
  EventListener::ID id(browser_context, extension_id, sub_event_name,
                       render_process_id, web_view_instance_id,
                       worker_thread_id, service_worker_version_id);
  EventListener* listener = nullptr;

  // Check if the "handled" event was for an inactive listener (indicated by
  // having neither a render process nor service worker version). This happens
  // when we fail to dispatch an event to a lazy service worker listener.
  // In this case, we still treat the event as handled, because otherwise the
  // request will hang indefinitely.
  if (render_process_id == -1 &&
      service_worker_version_id ==
          blink::mojom::kInvalidServiceWorkerVersionId) {
    listener = FindEventListenerInContainer(
        id, context_data.inactive_listeners[event_name]);
  } else {
    listener = FindEventListenerInContainer(
        id, context_data.active_listeners[event_name]);
  }

  // This might happen, for example, if the extension has been unloaded.
  if (!listener) {
    return;
  }

  listener->blocked_requests.erase(request_id);
  DecrementBlockCount(browser_context, extension_id, event_name, request_id,
                      std::move(response), listener->extra_info_spec);
}

bool UrlRequestEventRouter::AddEventListener(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id,
    const std::string& extension_name,
    const std::string& event_name,
    const std::string& sub_event_name,
    RequestFilter filter,
    int render_process_id,
    int web_view_instance_id,
    int worker_thread_id,
    int64_t service_worker_version_id) {
  if (!IsUrlRequestEvent(event_name)) {
    return false;
  }

  if (event_name != EventRouter::GetBaseEventName(sub_event_name)) {
    return false;
  }

  EventListener::ID id(browser_context, extension_id, sub_event_name,
                       render_process_id, web_view_instance_id,
                       worker_thread_id, service_worker_version_id);
  if (FindEventListener(id) != nullptr) {
    // This is likely an abuse of the API by a malicious extension.
    return false;
  }

  std::unique_ptr<EventListener> listener =
      std::make_unique<EventListener>(std::move(id));
  listener->extension_name = extension_name;
  listener->histogram_value = GetEventHistogramValue(event_name);
  listener->filter = std::move(filter);
  if (web_view_instance_id) {
    base::RecordAction(
        base::UserMetricsAction("WebView.UrlRequest.AddListener"));
  }

  BrowserContextID browser_context_id = GetBrowserContextID(browser_context);

  // This might be a reactivated listener - a listener being added for a
  // lazy context where it was shut down and then respawned. This can only
  // happen for service worker listeners.
  // bool is_reactivated = false;
  if (service_worker_version_id !=
      blink::mojom::kInvalidServiceWorkerVersionId) {
    auto& listeners = data_[browser_context_id].inactive_listeners[event_name];
    // Search for any listener with the same sub-event name.
    // NOTE: The sub-event name will be the same for any listener registered in
    // the *same order* in the extension. In practice, this should pretty much
    // always be the case, because we require listeners to be set up
    // synchronously.
    size_t erased = std::erase_if(
        listeners, [browser_context, extension_id, sub_event_name](
                       const std::unique_ptr<EventListener>& listener) {
          return listener->id.browser_context == browser_context &&
                 listener->id.extension_id == extension_id &&
                 listener->id.sub_event_name == sub_event_name;
        });
    // Only a single listener should ever match. It's possible no listener will
    // match if this is a new listener in a worker context.
    DCHECK_LE(erased, 1u);
  }

  data_[browser_context_id].active_listeners[event_name].push_back(
      std::move(listener));

  return true;
}

UrlRequestEventRouter::EventListener* UrlRequestEventRouter::FindEventListener(
    const EventListener::ID& id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string event_name = EventRouter::GetBaseEventName(id.sub_event_name);
  Listeners& listeners = data_[GetBrowserContextID(id.browser_context.get())]
                             .active_listeners[event_name];
  return FindEventListenerInContainer(id, listeners);
}

UrlRequestEventRouter::EventListener*
UrlRequestEventRouter::FindEventListenerInContainer(
    const EventListener::ID& id,
    const Listeners& listeners) {
  auto it = std::find_if(listeners.begin(), listeners.end(),
                         [&id](const auto& entry) { return entry->id == id; });
  return it != listeners.end() ? it->get() : nullptr;
}

// static
std::unique_ptr<UrlRequestEventRouter::EventListener>
UrlRequestEventRouter::RemoveMatchingListener(
    Listeners& listeners,
    const ExtensionId& extension_id,
    const std::string& sub_event_name,
    std::optional<int> worker_thread_id,
    std::optional<int64_t> service_worker_version_id,
    BrowserContextID browser_context_id) {
  Listeners removed_listeners;
  for (auto iter = listeners.begin(); iter != listeners.end();) {
    std::unique_ptr<EventListener>& listener = *iter;
    const EventListener::ID& id = listener->id;
    DCHECK_EQ(browser_context_id,
              GetBrowserContextID(id.browser_context.get()));
    bool listener_matches =
        extension_id == id.extension_id &&
        sub_event_name == id.sub_event_name &&
        (!worker_thread_id || worker_thread_id == id.worker_thread_id) &&
        (!service_worker_version_id ||
         service_worker_version_id == id.service_worker_version_id);
    if (!listener_matches) {
      ++iter;
      continue;
    }

    removed_listeners.push_back(std::move(listener));
    iter = listeners.erase(iter);
  }

  DCHECK_LE(removed_listeners.size(), 1u);
  return removed_listeners.empty() ? nullptr
                                   : std::move(removed_listeners.front());
}

void UrlRequestEventRouter::RemoveLazyListener(
    content::BrowserContext* original_context,
    const ExtensionId& extension_id,
    const std::string& sub_event_name) {
  std::string event_name = EventRouter::GetBaseEventName(sub_event_name);

  BrowserContextID original_context_id = GetBrowserContextID(original_context);

  // Remove any active or inactive listeners that match the sub-event name.
  // Due to https://crbug.com/1347597, we only have a single lazy listener
  // registration shared for both the on- and off-the-record contexts, so we
  // need to remove it from both. This means there may be a listener in any
  // of our four sets (active and inactive for both the on-the-record and
  // off-the-record contexts).
  BrowserContextData& data = data_[original_context_id];
  Listeners removed_listeners;
  auto check_list = [&removed_listeners, extension_id, sub_event_name](
                        Listeners& listeners,
                        BrowserContextID browser_context_id) {
    auto listener =
        RemoveMatchingListener(listeners, extension_id, sub_event_name,
                               std::nullopt, std::nullopt, browser_context_id);
    if (listener) {
      removed_listeners.push_back(std::move(listener));
    }
  };

  check_list(data.active_listeners[event_name], original_context_id);
  check_list(data.inactive_listeners[event_name], original_context_id);
  content::BrowserContext* const cross_context =
      GetCrossBrowserContext(original_context);
  if (cross_context) {
    BrowserContextID cross_context_id = GetBrowserContextID(cross_context);
    BrowserContextData& cross_data = data_[cross_context_id];
    check_list(cross_data.active_listeners[event_name], cross_context_id);
    check_list(cross_data.inactive_listeners[event_name], cross_context_id);
  }

  // We should only have a maximum of two listeners removed - one for the
  // on-the-record and one for the off-the-record profile - since each listener
  // must either be active *or* inactive.
  DCHECK_LE(removed_listeners.size(), 2u);

  for (const auto& listener : removed_listeners) {
    CleanUpForListener(*listener, ListenerUpdateType::kRemove);
  }
}

void UrlRequestEventRouter::UpdateActiveListener(
    content::BrowserContext* browser_context,
    ListenerUpdateType update_type,
    const ExtensionId& extension_id,
    const std::string& sub_event_name,
    int worker_thread_id,
    int64_t service_worker_version_id) {
  std::string event_name = EventRouter::GetBaseEventName(sub_event_name);

  const BrowserContextID browser_context_id =
      GetBrowserContextID(browser_context);
  BrowserContextData& data = data_[browser_context_id];
  auto matching_listener = RemoveMatchingListener(
      data.active_listeners[event_name], extension_id, sub_event_name,
      worker_thread_id, service_worker_version_id, browser_context_id);
  if (!matching_listener) {
    return;
  }

  CleanUpForListener(*matching_listener, update_type);

  // If this is only deactivating the listener, reset the process-specific bits
  // for the listener and move it to inactive listeners.
  if (update_type == ListenerUpdateType::kDeactivate) {
    matching_listener->id.worker_thread_id = kMainThreadId;
    matching_listener->id.service_worker_version_id =
        blink::mojom::kInvalidServiceWorkerVersionId;
    matching_listener->id.render_process_id = -1;
    data.inactive_listeners[event_name].push_back(std::move(matching_listener));
  }
}

void UrlRequestEventRouter::CleanUpForListener(const EventListener& listener,
                                               ListenerUpdateType update_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::string event_name =
      EventRouter::GetBaseEventName(listener.id.sub_event_name);

  // Unblock any request that this event listener may have been blocking.
  // Note that we do this even for deactivations, since if the service worker
  // is shut down (which would happen if it reached the hard lifetime timeout),
  // it won't be able to respond to the request.
  // TODO(crbug.com/40107353): This likely won't be sufficient, since it
  // means requests can leak through.
  for (uint64_t blocked_request_id : listener.blocked_requests) {
    DecrementBlockCount(listener.id.browser_context, listener.id.extension_id,
                        event_name, blocked_request_id, nullptr,
                        0 /* extra_info_spec */);
  }

  // Update the extra headers count and clear the cache only if the listener is
  // fully removed; otherwise, these values are still correct.
  if (update_type == ListenerUpdateType::kRemove) {
    // if (listener.HasExtraHeaders()) {
    //   DecrementExtraHeadersListenerCount(listener.id.browser_context);
    // }
    helpers::ClearCacheOnNavigation();
  }
}

void UrlRequestEventRouter::RemoveWebViewEventListeners(
    content::BrowserContext* browser_context,
    int render_process_id,
    int web_view_instance_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Iterate over all listeners of all UrlRequest events to delete
  // any listeners that belong to the provided <webview>.
  BrowserContextData& data = data_[GetBrowserContextID(browser_context)];
  for (auto& kv : data.active_listeners) {
    Listeners& listeners = kv.second;
    for (auto iter = listeners.begin(); iter != listeners.end();) {
      std::unique_ptr<EventListener>& listener = *iter;
      bool listener_matches =
          listener->id.render_process_id == render_process_id &&
          listener->id.web_view_instance_id == web_view_instance_id;
      if (!listener_matches) {
        ++iter;
        continue;
      }
      CleanUpForListener(**iter, ListenerUpdateType::kRemove);
      iter = listeners.erase(iter);
    }
  }
}

// static
void UrlRequestEventRouter::OnOTRBrowserContextCreated(
    content::BrowserContext* original_browser_context,
    content::BrowserContext* otr_browser_context) {
  CrossContextData::Get().AddContext(original_browser_context,
                                     otr_browser_context);
}

// static
void UrlRequestEventRouter::OnOTRBrowserContextDestroyed(
    content::BrowserContext* original_browser_context,
    content::BrowserContext* otr_browser_context) {
  ClearCrossContextData(original_browser_context);
  UrlRequestEventRouter* event_router =
      UrlRequestEventRouter::Get(original_browser_context);
  // Check if we get an instance before calling OnBrowserContextShutdown.
  // This is a workaround for tests that manipulate BrowserContext instances in
  // ways that break the expectations we have in production code.
  if (event_router) {
    event_router->OnBrowserContextShutdown(otr_browser_context);
    DCHECK(!base::Contains(event_router->data_,
                           GetBrowserContextID(otr_browser_context)));
  }
}

void UrlRequestEventRouter::AddCallbackForPageLoad(base::OnceClosure callback) {
  GetCallbacksForPageLoad().push_back(std::move(callback));
}

void UrlRequestEventRouter::OnBrowserContextShutdown(
    content::BrowserContext* browser_context) {
  data_.erase(GetBrowserContextID(browser_context));
}

size_t UrlRequestEventRouter::GetListenerCountForTesting(
    content::BrowserContext* browser_context,
    const std::string& event_name) {
  return data_[GetBrowserContextID(browser_context)]
      .active_listeners[event_name]
      .size();
}

size_t UrlRequestEventRouter::GetInactiveListenerCountForTesting(
    content::BrowserContext* browser_context,
    const std::string& event_name) {
  return data_[GetBrowserContextID(browser_context)]
      .inactive_listeners[event_name]
      .size();
}

UrlRequestEventRouter::BlockedRequestMap&
UrlRequestEventRouter::GetBlockedRequestMap(
    content::BrowserContext* browser_context) {
  // Blocked requests are stored in the data for the regular context.
  // TODO(crbug.com/40279375): Blocked requests should be isolated to
  // a particular BrowserContext and not shared between the main and
  // OTR contexts.
  if (browser_context->IsOffTheRecord()) {
    browser_context = GetCrossBrowserContext(browser_context);
  }
  return data_[GetBrowserContextID(browser_context)].blocked_requests;
}

void UrlRequestEventRouter::ClearBlockedRequest(
    content::BrowserContext* browser_context,
    uint64_t id) {
  GetBlockedRequestMap(browser_context).erase(id);
}

UrlRequestEventRouter::BlockedRequest&
UrlRequestEventRouter::GetOrAddBlockedRequest(
    content::BrowserContext* browser_context,
    uint64_t id) {
  return GetBlockedRequestMap(browser_context)[id];
}

UrlRequestEventRouter::BlockedRequest* UrlRequestEventRouter::GetBlockedRequest(
    content::BrowserContext* browser_context,
    uint64_t id) {
  BlockedRequestMap& blocked_requests = GetBlockedRequestMap(browser_context);
  auto it = blocked_requests.find(id);
  return it == blocked_requests.end() ? nullptr : &it->second;
}

bool UrlRequestEventRouter::IsPageLoad(const UrlRequestInfo& request) const {
  return request.url_request_type == UrlRequestResourceType::MAIN_FRAME;
}

void UrlRequestEventRouter::NotifyPageLoad() {
  for (auto& callback : GetCallbacksForPageLoad()) {
    std::move(callback).Run();
  }
  GetCallbacksForPageLoad().clear();
}

// static
content::BrowserContext* UrlRequestEventRouter::GetCrossBrowserContext(
    content::BrowserContext* browser_context) {
  return CrossContextData::Get().GetCrossBrowserContext(browser_context);
}

bool UrlRequestEventRouter::WasSignaled(
    content::BrowserContext* browser_context,
    uint64_t request_id) const {
  const SignaledRequestIDTracker* const tracker =
      GetSignaledRequestIDTracker(browser_context);
  return !tracker ? false : tracker->WasSignaled(request_id);
}

UrlRequestEventRouter::RawListeners UrlRequestEventRouter::GetMatchingListeners(
    content::BrowserContext* browser_context,
    const std::string& event_name,
    const UrlRequestInfo* request,
    int* extra_info_spec) {
  *extra_info_spec = 0;

  bool is_request_from_extension =
      IsRequestFromExtension(*request, browser_context);

  std::string url_request_event_name(event_name);
  if (request->is_web_view) {
    url_request_event_name.replace(0, kUrlRequestEventPrefixLen,
                                   kWebViewEventPrefix);
  }

  RawListeners matching_listeners;

  auto& browser_context_data = data_[GetBrowserContextID(browser_context)];

  GetMatchingListenersForRequest(
      browser_context_data.active_listeners[url_request_event_name], *request,
      *browser_context, is_request_from_extension,
      /*crosses_incognito=*/false, &matching_listeners, extra_info_spec);
  GetMatchingListenersForRequest(
      browser_context_data.inactive_listeners[url_request_event_name], *request,
      *browser_context, is_request_from_extension,
      /*crosses_incognito=*/false, &matching_listeners, extra_info_spec);

  content::BrowserContext* cross_browser_context =
      GetCrossBrowserContext(browser_context);
  if (cross_browser_context) {
    auto& cross_context_data =
        data_[GetBrowserContextID(cross_browser_context)];
    GetMatchingListenersForRequest(
        cross_context_data.active_listeners[url_request_event_name], *request,
        *cross_browser_context, is_request_from_extension,
        /*crosses_incognito=*/true, &matching_listeners, extra_info_spec);
    GetMatchingListenersForRequest(
        cross_context_data.inactive_listeners[url_request_event_name], *request,
        *cross_browser_context, is_request_from_extension,
        /*crosses_incognito=*/true, &matching_listeners, extra_info_spec);
  }

  return matching_listeners;
}

// static
bool UrlRequestEventRouter::ListenerMatchesRequest(
    const EventListener& listener,
    const UrlRequestInfo& request,
    content::BrowserContext& browser_context,
    bool is_request_from_extension,
    bool crosses_incognito) {
  if (!content::RenderProcessHost::FromID(listener.id.render_process_id) &&
      listener.id.service_worker_version_id >= 0) {
    // The IPC sender has been deleted. This listener will be removed soon
    // via a call to `CleanUpForListener()`. For now, just skip it.
    return false;
  }

  if (request.is_web_view) {
    // If this is a navigation request, then we can skip this check. IDs will
    // be -1 and the request is trusted.
    if (!request.is_navigation_request &&
        listener.id.render_process_id != request.web_view_embedder_process_id) {
      return false;
    }

    if (listener.id.web_view_instance_id != request.web_view_instance_id) {
      return false;
    }
  }

  // Filter requests from other extensions / apps. This does not work for
  // content scripts, or extension pages in non-extension processes.
  if (is_request_from_extension &&
      listener.id.render_process_id != request.render_process_id) {
    return false;
  }

  if (!listener.filter.urls.is_empty() &&
      !listener.filter.urls.MatchesURL(request.url)) {
    return false;
  }

  // Check if the tab id and window id match, if they were set in the
  // listener params.
  if ((listener.filter.tab_id != -1 &&
       request.frame_data.tab_id != listener.filter.tab_id) ||
      (listener.filter.window_id != -1 &&
       request.frame_data.window_id != listener.filter.window_id)) {
    return false;
  }

  const std::vector<UrlRequestResourceType>& types = listener.filter.types;
  if (!types.empty() && !base::Contains(types, request.url_request_type)) {
    return false;
  }

  if (!request.is_web_view) {
    PermissionsData::PageAccess access =
        UrlRequestPermissions::CanExtensionAccessURL(
            UrlRequestPermissionHelper::Get(&browser_context), listener.id.extension_id,
            request.url, request.frame_data.tab_id, crosses_incognito,
            UrlRequestPermissions::
                REQUIRE_HOST_PERMISSION_FOR_URL_AND_INITIATOR,
            request.initiator, request.url_request_type);

    if (access != PermissionsData::PageAccess::kAllowed) {
      if (access == PermissionsData::PageAccess::kWithheld) {
        DCHECK(ExtensionsAPIClient::Get());
        // ExtensionsAPIClient::Get()->NotifyUrlRequestWithheld( // NotifyWebRequestWithheld did nothing anyway
        //     request.render_process_id, request.frame_routing_id,
        //     listener.id.extension_id);
      }

      return false;
    }
  }

  // We do not want to notify extensions about XHR requests that are
  // triggered by themselves. This is a workaround to prevent deadlocks
  // in case of synchronous XHR requests that block the extension renderer
  // and therefore prevent the extension from processing the request
  // handler. This is only a problem for blocking listeners.
  // http://crbug.com/105656
  bool synchronous_xhr_from_extension =
      !request.is_async && is_request_from_extension &&
      request.url_request_type == UrlRequestResourceType::XHR;
  return /* !listener.IsBlocking() || */ !synchronous_xhr_from_extension;
}

// static
void UrlRequestEventRouter::GetMatchingListenersForRequest(
    const Listeners& listeners,
    const UrlRequestInfo& request,
    content::BrowserContext& browser_context,
    bool is_request_from_extension,
    bool crosses_incognito,
    RawListeners* listeners_out,
    int* extra_info_spec_out) {
  for (const auto& listener : listeners) {
    if (ListenerMatchesRequest(*listener, request, browser_context,
                               is_request_from_extension, crosses_incognito)) {
      listeners_out->push_back(listener.get());
      *extra_info_spec_out |= listener->extra_info_spec;
    }
  }
}

void UrlRequestEventRouter::DecrementBlockCount(
    content::BrowserContext* browser_context,
    const ExtensionId& extension_id,
    const std::string& event_name,
    uint64_t request_id,
    std::unique_ptr<EventResponse> response,
    int extra_info_spec) {
  // It's possible that this request was deleted, or cancelled by a previous
  // event handler or handled by Declarative Net Request API. If so, ignore this
  // response.
  BlockedRequest* blocked_request =
      GetBlockedRequest(browser_context, request_id);
  if (!blocked_request) {
    return;
  }

  // Ensure that the response is for the event we are blocked on.
  DCHECK_EQ(blocked_request->event, GetEventTypeFromEventName(event_name));
  // Cache the event type; we use it below.
  EventTypes request_event = blocked_request->event;

  int num_handlers_blocking = --blocked_request->num_handlers_blocking;
  CHECK_GE(num_handlers_blocking, 0);

  if (response) {
    helpers::EventResponseDelta delta = CalculateDelta(
        browser_context, blocked_request, response.get(), extra_info_spec);
    // activity_monitor::OnUrlRequestApiUsed(
    //     static_cast<content::BrowserContext*>(browser_context), extension_id,
    //     blocked_request->request->url, blocked_request->is_incognito,
    //     event_name, SummarizeResponseDelta(event_name, delta));

    blocked_request->response_deltas.push_back(std::move(delta));
  }

  if (num_handlers_blocking == 0) {
    ExecuteDeltas(browser_context, blocked_request->request, true);
    // Note: `blocked_request` can be deleted here, depending on the outcome
    // of ExecuteDeltas(). Use the cached `request_event` and `request_id`
    // instead of using `blocked_request`.
    if (request_event == kOnBeforeRequest) {
      GetExtensionUrlRequestTimeTracker().LogBeforeRequestCompletionTime(
          request_id, base::TimeTicks::Now());
    }
  }
}

void UrlRequestEventRouter::SendMessages(
    content::BrowserContext* browser_context,
    const BlockedRequest& blocked_request) {
  const helpers::EventResponseDeltas& deltas = blocked_request.response_deltas;
  for (const auto& delta : deltas) {
    const std::set<std::string>& messages = delta.messages_to_extension;
    for (const std::string& message : messages) {
      std::unique_ptr<UrlRequestEventDetails> event_details(CreateEventDetails(
          *blocked_request.request, /* extra_info_spec */ 0));
      event_details->SetString(keys::kMessageKey, message);
      event_details->SetString(keys::kStageKey,
                               GetRequestStageAsString(blocked_request.event));
      SendOnMessageEventOnUI(browser_context, delta.extension_id,
                             blocked_request.request->is_web_view,
                             blocked_request.request->web_view_instance_id,
                             std::move(event_details));
    }
  }
}

int UrlRequestEventRouter::ExecuteDeltas(
    content::BrowserContext* browser_context,
    const UrlRequestInfo* request,
    bool call_callback) {
  BlockedRequest& blocked_request =
      GetOrAddBlockedRequest(browser_context, request->id);
  CHECK_EQ(0, blocked_request.num_handlers_blocking);
  helpers::EventResponseDeltas& deltas = blocked_request.response_deltas;
  base::TimeDelta block_time =
      base::Time::Now() - blocked_request.blocking_time;
  GetExtensionUrlRequestTimeTracker().IncrementTotalBlockTime(request->id,
                                                              block_time);

  bool request_headers_modified = false;
  bool response_headers_modified = false;
  // The set of request headers which were removed or set to new values.
  std::set<std::string> request_headers_removed;
  std::set<std::string> request_headers_set;

  deltas.sort(&helpers::InDecreasingExtensionInstallationTimeOrder);

  std::optional<ExtensionId> canceled_by_extension;
  std::optional<ExtensionId> finished_by_extension;
  helpers::MergeCancelOfResponses(blocked_request.response_deltas,
                                  &canceled_by_extension, &finished_by_extension);

  extension_url_request_api_helpers::IgnoredActions ignored_actions;
  if (blocked_request.event == kOnBeforeRequest) {
    CHECK(!blocked_request.callback.is_null());
    helpers::MergeOnBeforeRequestResponses(
        request->url, blocked_request.response_deltas, blocked_request.new_url, blocked_request.response,
        &ignored_actions);
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  SendMessages(browser_context, blocked_request);

  if (!ignored_actions.empty()) {
    // TODO !!!
    // NotifyIgnoredActionsOnUI(browser_context, request->id,
    //                          std::move(ignored_actions));
  }

  const bool redirected =
      blocked_request.new_url && !blocked_request.new_url->is_empty();

  if (canceled_by_extension) {
    GetExtensionUrlRequestTimeTracker().SetRequestCanceled(request->id);
  } else if (redirected) {
    GetExtensionUrlRequestTimeTracker().SetRequestRedirected(request->id);
  }

  // Log UMA metrics. Note: We are not necessarily concerned with the final
  // action taken. Instead we are interested in how frequently the different
  // actions are used by extensions. Hence multiple actions may be logged for a
  // single delta execution.
  if (canceled_by_extension) {
    LogRequestAction(RequestAction::CANCEL);
  }
  if (redirected) {
    LogRequestAction(RequestAction::REDIRECT);
  }
  if (request_headers_modified) {
    LogRequestAction(RequestAction::MODIFY_REQUEST_HEADERS);
  }
  if (response_headers_modified) {
    LogRequestAction(RequestAction::MODIFY_RESPONSE_HEADERS);
  }

  // This triggers onErrorOccurred if canceled is true.
  int rv = net::OK;
  if (canceled_by_extension) {
    rv = net::ERR_BLOCKED_BY_CLIENT;
    TRACE_EVENT2("extensions", "NetworkRequestBlockedByClient", "extension",
                 canceled_by_extension.value(), "id", request->id);
  }

  if (!blocked_request.callback.is_null()) {
    net::CompletionOnceCallback callback = std::move(blocked_request.callback);
    // Ensure that request is removed before callback because the callback
    // might trigger the next event.
    ClearBlockedRequest(browser_context, request->id);
    if (call_callback) {
      std::move(callback).Run(rv);
    }
  } else {
    ClearBlockedRequest(browser_context, request->id);
  }
  return rv;
}

bool UrlRequestEventRouter::GetAndSetSignaled(
    content::BrowserContext* browser_context,
    uint64_t request_id,
    EventTypes event_type) {
  return GetSignaledRequestIDTracker(browser_context)
      .GetAndSet(request_id, event_type);
}

void UrlRequestEventRouter::ClearSignaled(
    content::BrowserContext* browser_context,
    uint64_t request_id,
    EventTypes event_type) {
  GetSignaledRequestIDTracker(browser_context)
      .ClearEventType(request_id, event_type);
}

}  // namespace extensions
