// Copyright 2012 The Chromium Authors
// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/url_request/url_request_api.h"
#include "extensions/browser/api/url_request/url_request_proxying_url_loader_factory.h"

#include "base/lazy_instance.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "components/safe_browsing/core/common/features.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/extension_api.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/features/feature_provider.h"
#include "extensions/browser/browser_frame_context_data.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/process_map.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#endif

using content::BrowserThread;
using extensions::mojom::APIPermissionID;

namespace helpers = extension_url_request_api_helpers;
namespace keys = extension_url_request_api_constants;

using URLLoaderFactoryType =
    content::ContentBrowserClient::URLLoaderFactoryType;

namespace extensions {

namespace url_request = api::url_request;

namespace {
// Converts an HttpHeaders dictionary to a |name|, |value| pair. Returns
// true if successful.
bool FromHeaderDictionary(const base::Value::Dict& header_value,
                          std::string* name,
                          std::string* out_value) {
  const std::string* name_ptr = header_value.FindString(keys::kHeaderNameKey);
  if (!name) {
    return false;
  }
  *name = *name_ptr;

  const base::Value* value = header_value.Find(keys::kHeaderValueKey);
  const base::Value* binary_value =
      header_value.Find(keys::kHeaderBinaryValueKey);
  // We require either a "value" or a "binaryValue" entry, but not both.
  if ((value == nullptr && binary_value == nullptr) ||
      (value != nullptr && binary_value != nullptr)) {
    return false;
  }

  if (value) {
    if (!value->is_string()) {
      return false;
    }
    *out_value = value->GetString();
  } else if (!binary_value->is_list() ||
             !helpers::CharListToString(binary_value->GetList(), out_value)) {
    return false;
  }
  return true;
}

// Checks whether the extension has any permissions that would intercept or
// modify network requests.
bool HasAnyUrlRequestPermissions(const Extension* extension) {
  static constexpr APIPermissionID kUrlRequestPermissions[] = {
      APIPermissionID::kUrlRequest,
  };

  const PermissionsData* permissions = extension->permissions_data();
  for (auto permission : kUrlRequestPermissions) {
    if (permissions->HasAPIPermission(permission)) {
      return true;
    }
  }
  return false;
}

} // namespace

bool UrlRequestAPI::MayHaveProxies() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  return url_request_extension_count_ > 0;
}

bool UrlRequestAPI::IsAvailableToWebViewEmbedderFrame(
    content::RenderFrameHost* render_frame_host) const {
#if BUILDFLAG(ENABLE_GUEST_VIEW)
  if (!render_frame_host || !WebViewGuest::IsGuest(render_frame_host)) {
    return false;
  }

  content::BrowserContext* browser_context =
      render_frame_host->GetBrowserContext();
  content::RenderFrameHost* embedder_frame =
      render_frame_host->GetOutermostMainFrameOrEmbedder();

  if (!ProcessMap::Get(browser_context)
           ->CanProcessHostContextType(/*extension=*/nullptr,
                                       *embedder_frame->GetProcess(),
                                       mojom::ContextType::kWebPage)) {
    return false;
  }

  Feature::Availability availability =
      ExtensionAPI::GetSharedInstance()->IsAvailable(
          "urlRequestInternal", /*extension=*/nullptr,
          mojom::ContextType::kWebPage, embedder_frame->GetLastCommittedURL(),
          CheckAliasStatus::ALLOWED, util::GetBrowserContextId(browser_context),
          BrowserFrameContextData(embedder_frame));
  return availability.is_available();
#else
  return false;
#endif
}

void UrlRequestAPI::UpdateMayHaveProxies() {
  bool may_have_proxies = MayHaveProxies();
  if (!may_have_proxies_ && may_have_proxies) {
    browser_context_->GetDefaultStoragePartition()->ResetURLLoaderFactories();
  }
  may_have_proxies_ = may_have_proxies;
}

void UrlRequestAPI::OnExtensionLoaded(content::BrowserContext* browser_context,
                                      const Extension* extension) {
  if (HasAnyUrlRequestPermissions(extension)) {
    ++url_request_extension_count_;
    UpdateMayHaveProxies();
  }
}

void UrlRequestAPI::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionReason reason) {
  if (HasAnyUrlRequestPermissions(extension)) {
    --url_request_extension_count_;
    UpdateMayHaveProxies();
  }
}

UrlRequestAPI::ProxySet::ProxySet() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

UrlRequestAPI::ProxySet::~ProxySet() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void UrlRequestAPI::ProxySet::AddProxy(std::unique_ptr<Proxy> proxy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  proxies_.insert(std::move(proxy));
}

void UrlRequestAPI::ProxySet::RemoveProxy(Proxy* proxy) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto requests_it = proxy_to_request_id_map_.find(proxy);
  if (requests_it != proxy_to_request_id_map_.end()) {
    for (const auto& id : requests_it->second) {
      request_id_to_proxy_map_.erase(id);
    }
    proxy_to_request_id_map_.erase(requests_it);
  }

  auto proxy_it = proxies_.find(proxy);
  CHECK(proxy_it != proxies_.end(), base::NotFatalUntil::M130);
  proxies_.erase(proxy_it);
}

void UrlRequestAPI::ProxySet::AssociateProxyWithRequestId(
    Proxy* proxy,
    const content::GlobalRequestID& id) {
  DCHECK(proxy);
  DCHECK(proxies_.count(proxy));
  DCHECK(id.request_id);
  auto result = request_id_to_proxy_map_.emplace(id, proxy);
  DCHECK(result.second) << "Unexpected request ID collision.";
  proxy_to_request_id_map_[proxy].insert(id);
}

void UrlRequestAPI::ProxySet::DisassociateProxyWithRequestId(
    Proxy* proxy,
    const content::GlobalRequestID& id) {
  DCHECK(proxy);
  DCHECK(proxies_.count(proxy));
  DCHECK(id.request_id);
  size_t count = request_id_to_proxy_map_.erase(id);
  DCHECK_GT(count, 0u);
  count = proxy_to_request_id_map_[proxy].erase(id);
  DCHECK_GT(count, 0u);
}

UrlRequestAPI::Proxy* UrlRequestAPI::ProxySet::GetProxyFromRequestId(
    const content::GlobalRequestID& id) {
  auto it = request_id_to_proxy_map_.find(id);
  return it == request_id_to_proxy_map_.end() ? nullptr : it->second;
}

UrlRequestAPI::RequestIDGenerator::RequestIDGenerator() = default;
UrlRequestAPI::RequestIDGenerator::~RequestIDGenerator() = default;

int64_t UrlRequestAPI::RequestIDGenerator::Generate(
    int32_t routing_id,
    int32_t network_service_request_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it = saved_id_map_.find({routing_id, network_service_request_id});
  if (it != saved_id_map_.end()) {
    int64_t id = it->second;
    saved_id_map_.erase(it);
    return id;
  }
  return ++id_;
}

void UrlRequestAPI::RequestIDGenerator::SaveID(
    int32_t routing_id,
    int32_t network_service_request_id,
    uint64_t request_id) {
  // If |network_service_request_id| is 0, we cannot reliably match the
  // generated ID to a future request, so ignore it.
  if (network_service_request_id != 0) {
    saved_id_map_.insert(
        {{routing_id, network_service_request_id}, request_id});
  }
}

UrlRequestAPI::UrlRequestAPI(content::BrowserContext* context)
    : browser_context_(context),
      proxies_(std::make_unique<ProxySet>()) {

  EventRouter* event_router = EventRouter::Get(browser_context_);
  for (std::string event_name : UrlRequestEventRouter::GetEventNames()) {
    event_router->RegisterObserver(this, event_name);
  }
  extensions::ExtensionRegistry::Get(browser_context_)->AddObserver(this);
}

UrlRequestAPI::~UrlRequestAPI() = default;

static base::LazyInstance<BrowserContextKeyedAPIFactory<UrlRequestAPI>>::
    DestructorAtExit g_api_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<UrlRequestAPI>* UrlRequestAPI::GetFactoryInstance() {
  return g_api_factory.Pointer();
}

void UrlRequestAPI::OnListenerAdded(
    const EventListenerInfo& details) { }

void UrlRequestAPI::OnListenerRemoved(
    const EventListenerInfo& details) { }

bool UrlRequestAPI::MaybeProxyURLLoaderFactory(
    content::BrowserContext* browser_context,
    content::RenderFrameHost* frame,
    int render_process_id,
    URLLoaderFactoryType type,
    std::optional<int64_t> navigation_id,
    ukm::SourceIdObj ukm_source_id,
    network::URLLoaderFactoryBuilder& factory_builder,
    mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
        header_client,
    scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner,
    const url::Origin& request_initiator) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!MayHaveProxies()) {
    bool use_proxy = false;

#if BUILDFLAG(ENABLE_GUEST_VIEW)
    // There are a few internal WebUIs that use WebView tag that are allowlisted
    // for urlRequest.
    // TODO(crbug.com/40288053): Remove the scheme check once we're sure
    // that WebUIs with WebView run in real WebUI processes and check the
    // context type using |IsAvailableToWebViewEmbedderFrame()| below.
    if (WebViewGuest::IsGuest(frame)) {
      content::RenderFrameHost* embedder =
          frame->GetOutermostMainFrameOrEmbedder();
      const auto& embedder_url = embedder->GetLastCommittedURL();
      if (embedder_url.SchemeIs(content::kChromeUIScheme)) {
        auto* feature = FeatureProvider::GetAPIFeature("urlRequestInternal");
        if (feature
                ->IsAvailableToContext(
                    nullptr, mojom::ContextType::kWebUi, embedder_url,
                    util::GetBrowserContextId(browser_context),
                    BrowserFrameContextData(frame))
                .is_available()) {
          use_proxy = true;
        }
      } else {
        use_proxy = IsAvailableToWebViewEmbedderFrame(frame);
      }
    }
#endif

    // Create a proxy URLLoader even when there is no CRX
    // installed with urlRequest permissions. This allows the extension
    // requests to be intercepted for CRX telemetry service if enabled.
    // Only proxy if the new RHC interception logic is disabled.
    // TODO(crbug.com/40913716): Clean up collection logic here once new RHC
    // interception logic is fully launched.
    const std::string& request_scheme = request_initiator.scheme();
    if (extensions::kExtensionScheme == request_scheme &&
        ExtensionsBrowserClient::Get()->IsExtensionTelemetryServiceEnabled(
            browser_context) &&
        base::FeatureList::IsEnabled(
            safe_browsing::kExtensionTelemetryReportContactedHosts) &&
        !base::FeatureList::IsEnabled(
            safe_browsing::
                kExtensionTelemetryInterceptRemoteHostsContactedInRenderer)) {
      use_proxy = true;
    }
    if (!use_proxy) {
      return false;
    }
  }

  std::unique_ptr<ExtensionNavigationUIData> navigation_ui_data;
  const bool is_navigation = (type == URLLoaderFactoryType::kNavigation);
  if (is_navigation) {
    DCHECK(frame);
    DCHECK(navigation_id);
    int tab_id;
    int window_id;
    ExtensionsBrowserClient::Get()->GetTabAndWindowIdForWebContents(
        content::WebContents::FromRenderFrameHost(frame), &tab_id, &window_id);
    navigation_ui_data =
        std::make_unique<ExtensionNavigationUIData>(frame, tab_id, window_id);
  }

  mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
      header_client_receiver;
  if (header_client) {
    header_client_receiver = header_client->InitWithNewPipeAndPassReceiver();
  }

  // NOTE: This request may be proxied on behalf of an incognito frame, but
  // |this| will always be bound to a regular profile (see
  // |BrowserContextKeyedAPI::kServiceRedirectedInIncognito|).
  DCHECK(browser_context == browser_context_ ||
         (browser_context->IsOffTheRecord() &&
          ExtensionsBrowserClient::Get()->GetOriginalContext(browser_context) ==
              browser_context_));
  UrlRequestProxyingURLLoaderFactory::StartProxying(
      browser_context, is_navigation ? -1 : render_process_id,
      frame ? frame->GetRoutingID() : MSG_ROUTING_NONE,
      frame ? frame->GetRenderViewHost()->GetRoutingID() : MSG_ROUTING_NONE,
      &request_id_generator_, std::move(navigation_ui_data),
      std::move(navigation_id), ukm_source_id, factory_builder,
      std::move(header_client_receiver), proxies_.get(), type,
      std::move(navigation_response_task_runner));
  return true;
}

ExtensionFunction::ResponseAction
UrlRequestInternalAddEventListenerFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() == 5);

  // Argument 0 is the callback, which we don't use here.
  UrlRequestEventRouter::RequestFilter filter;
  EXTENSION_FUNCTION_VALIDATE(args()[1].is_dict());
  // Failure + an empty error string means a fatal error.
  std::string error;
  EXTENSION_FUNCTION_VALIDATE(
      filter.InitFromValue(args()[1].GetDict(), &error) || !error.empty());
  if (!error.empty()) {
    return RespondNow(Error(std::move(error)));
  }

  const auto& event_name_value = args()[2];
  const auto& sub_event_name_value = args()[3];
  const auto& web_view_instance_id_value = args()[4];
  EXTENSION_FUNCTION_VALIDATE(event_name_value.is_string());
  EXTENSION_FUNCTION_VALIDATE(sub_event_name_value.is_string());
  EXTENSION_FUNCTION_VALIDATE(web_view_instance_id_value.is_int());
  std::string event_name = event_name_value.GetString();
  std::string sub_event_name = sub_event_name_value.GetString();
  int web_view_instance_id = web_view_instance_id_value.GetInt();

  int render_process_id = source_process_id();

  const Extension* extension = ExtensionRegistry::Get(browser_context())
                                   ->enabled_extensions()
                                   .GetByID(extension_id_safe());
  std::string extension_name =
      extension ? extension->name() : extension_id_safe();

  if (web_view_instance_id) {
    // If a web view ID has been supplied and the call is from an extension
    // (i.e. not from WebUI), we require the extension to have the webview
    // permission.
    if (extension && !extension->permissions_data()->HasAPIPermission(
                         mojom::APIPermissionID::kWebView)) {
      return RespondNow(Error("Missing webview permission."));
    }
  } else {
    // We allow to subscribe to patterns that are broader than the host
    // permissions. E.g., we could subscribe to http://www.example.com/*
    // while having host permissions for http://www.example.com/foo/* and
    // http://www.example.com/bar/*.
    // For this reason we do only a coarse check here to warn the extension
    // developer if they do something obviously wrong.
    if (extension->permissions_data()
            ->GetEffectiveHostPermissions()
            .is_empty() &&
        extension->permissions_data()
            ->withheld_permissions()
            .explicit_hosts()
            .is_empty()) {
      return RespondNow(Error(keys::kHostPermissionsRequired));
    }
  }

  bool success =
      UrlRequestEventRouter::Get(browser_context())
          ->AddEventListener(browser_context(), extension_id_safe(),
                             extension_name, event_name, sub_event_name,
                             std::move(filter),
                             render_process_id, web_view_instance_id,
                             worker_thread_id(), service_worker_version_id());
  EXTENSION_FUNCTION_VALIDATE(success);

  helpers::ClearCacheOnNavigation();

  return RespondNow(NoArguments());
}

void UrlRequestInternalEventHandledFunction::OnError(
    const std::string& event_name,
    const std::string& sub_event_name,
    uint64_t request_id,
    int render_process_id,
    int web_view_instance_id,
    std::unique_ptr<UrlRequestEventRouter::EventResponse> response) {
  UrlRequestEventRouter::Get(browser_context())
      ->OnEventHandled(browser_context(), extension_id_safe(), event_name,
                       sub_event_name, request_id, render_process_id,
                       web_view_instance_id, worker_thread_id(),
                       service_worker_version_id(), std::move(response));
}

ExtensionFunction::ResponseAction
UrlRequestInternalEventHandledFunction::Run() {
  EXTENSION_FUNCTION_VALIDATE(args().size() >= 5);
  const auto& event_name_value = args()[0];
  const auto& sub_event_name_value = args()[1];
  const auto& request_id_str_value = args()[2];
  const auto& web_view_instance_id_value = args()[3];
  EXTENSION_FUNCTION_VALIDATE(event_name_value.is_string());
  EXTENSION_FUNCTION_VALIDATE(sub_event_name_value.is_string());
  EXTENSION_FUNCTION_VALIDATE(request_id_str_value.is_string());
  EXTENSION_FUNCTION_VALIDATE(web_view_instance_id_value.is_int());
  std::string event_name = event_name_value.GetString();
  std::string sub_event_name = sub_event_name_value.GetString();
  std::string request_id_str = request_id_str_value.GetString();
  int web_view_instance_id = web_view_instance_id_value.GetInt();

  uint64_t request_id;
  EXTENSION_FUNCTION_VALIDATE(
      base::StringToUint64(request_id_str, &request_id));

  int render_process_id = source_process_id();

  std::unique_ptr<UrlRequestEventRouter::EventResponse> response;
  if (HasOptionalArgument(4)) {
    EXTENSION_FUNCTION_VALIDATE(args()[4].is_dict());
    const base::Value::Dict& dict_value = args()[4].GetDict();

    if (!dict_value.empty()) {
      base::Time install_time = ExtensionPrefs::Get(browser_context())
                                    ->GetLastUpdateTime(extension_id_safe());
      response = std::make_unique<UrlRequestEventRouter::EventResponse>(
          extension_id_safe(), install_time);
    }

    const base::Value* redirect_url_value = dict_value.Find("redirectUrl");
    const base::Value* cancel_value = dict_value.Find("cancel");
    const base::Value* response_value = dict_value.Find("response");

    if (cancel_value) {
      // Don't allow cancel mixed with other keys.
      if (dict_value.size() != 1) {
        OnError(event_name, sub_event_name, request_id, render_process_id,
                web_view_instance_id, std::move(response));
        return RespondNow(Error(keys::kInvalidBlockingResponse));
      }

      EXTENSION_FUNCTION_VALIDATE(cancel_value->is_bool());
      response->cancel = cancel_value->GetBool();
    }

    if (response_value) {
      // Don't allow response mixed with other keys.
      if (dict_value.size() != 1) {
        OnError(event_name, sub_event_name, request_id, render_process_id,
                web_view_instance_id, std::move(response));
        return RespondNow(Error(keys::kInvalidBlockingResponse));
      }
    
      EXTENSION_FUNCTION_VALIDATE(response_value->is_dict());
      const base::Value::Dict&  response_dict = response_value->GetDict();

      auto* body = response_dict.Find("body");
      EXTENSION_FUNCTION_VALIDATE(body->is_blob());
      response->response.body = body->GetBlob();

      auto* status = response_dict.Find("status");
      EXTENSION_FUNCTION_VALIDATE(status->is_string());
      response->response.status = status->GetString();

      auto* status_text = response_dict.Find("statusText");
      EXTENSION_FUNCTION_VALIDATE(status_text->is_string());
      response->response.status_text = status_text->GetString();

      const base::Value* blocking_response_headers_value =
        response_dict.Find("headers");

      const bool has_response_headers = blocking_response_headers_value != nullptr;
      if (has_response_headers) {

        const base::Value::List* headers_value = nullptr;
        std::unique_ptr<helpers::ResponseHeaders> response_headers;
        
        response_headers = std::make_unique<helpers::ResponseHeaders>();
        headers_value = response_dict.FindList("headers");
        EXTENSION_FUNCTION_VALIDATE(headers_value);
        // headers with binary value??? handled by FromHeaderDictionary
        for (const base::Value& elem : *headers_value) {
          EXTENSION_FUNCTION_VALIDATE(elem.is_dict());
          const base::Value::Dict& header_value = elem.GetDict();
          std::string name;
          std::string value;
          if (!FromHeaderDictionary(header_value, &name, &value)) {
            std::string serialized_header;
            base::JSONWriter::Write(header_value, &serialized_header);
            OnError(event_name, sub_event_name, request_id, render_process_id,
                    web_view_instance_id, std::move(response));
            return RespondNow(Error(keys::kInvalidHeader, serialized_header));
          }
          if (!net::HttpUtil::IsValidHeaderName(name)) {
            OnError(event_name, sub_event_name, request_id, render_process_id,
                    web_view_instance_id, std::move(response));
            return RespondNow(Error(keys::kInvalidHeaderName));
          }
          if (!net::HttpUtil::IsValidHeaderValue(value)) {
            OnError(event_name, sub_event_name, request_id, render_process_id,
                    web_view_instance_id, std::move(response));
            return RespondNow(Error(keys::kInvalidHeaderValue, name));
          }
          response_headers->push_back(helpers::ResponseHeader(name, value));
        }
        response->response.headers = *response_headers.release();
      }
    }

    if (redirect_url_value) {
      EXTENSION_FUNCTION_VALIDATE(redirect_url_value->is_string());
      std::string new_url_str = redirect_url_value->GetString();
      response->new_url = GURL(new_url_str);
      if (!response->new_url.is_valid()) {
        OnError(event_name, sub_event_name, request_id, render_process_id,
                web_view_instance_id, std::move(response));
        return RespondNow(Error(keys::kInvalidRedirectUrl, new_url_str));
      }
    }
  }

  UrlRequestEventRouter::Get(browser_context())
      ->OnEventHandled(browser_context(), extension_id_safe(), event_name,
                       sub_event_name, request_id, render_process_id,
                       web_view_instance_id, worker_thread_id(),
                       service_worker_version_id(), std::move(response));

  return RespondNow(NoArguments());
}

}  // namespace extensions
