// Copyright 2018 The Chromium Authors
// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/url_request/url_request_proxying_url_loader_factory.h"

#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/types/optional_util.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/keyed_service/core/keyed_service_shutdown_notifier.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_utils.h"
#include "extensions/browser/api/url_request/extension_url_request_event_router.h"
#include "extensions/browser/api/url_request/url_request_permission_helper.h"
#include "extensions/browser/api/url_request/url_request_api.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/extensions_client.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "net/base/auth.h"
#include "net/base/ip_endpoint.h"
#include "net/base/isolation_info.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/redirect_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/parsed_headers.mojom-forward.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace extensions {
namespace {

// TODO(crbug.com/40768738): Consider removing traces when the cause of the
// issue is identified.
constexpr char kUrlRequestProxyingURLLoaderFactoryScope[] =
    "UrlRequestProxyingURLLoaderFactory";

// This shutdown notifier makes sure the proxy is destroyed if an incognito
// browser context is destroyed. This is needed because UrlRequestAPI only
// clears the proxies when the original browser context is destroyed.
class ShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  ShutdownNotifierFactory(const ShutdownNotifierFactory&) = delete;
  ShutdownNotifierFactory& operator=(const ShutdownNotifierFactory&) = delete;

  static ShutdownNotifierFactory* GetInstance() {
    static base::NoDestructor<ShutdownNotifierFactory> factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<ShutdownNotifierFactory>;

  ShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "UrlRequestProxyingURLLoaderFactory") {
    DependsOn(UrlRequestPermissionHelper::GetFactoryInstance());
  }
  ~ShutdownNotifierFactory() override {}

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    return ExtensionsBrowserClient::Get()->GetContextOwnInstance(
        context, /*force_guest_profile=*/true);
  }
};

// Creates simulated net::RedirectInfo when an extension redirects a request,
// behaving like a redirect response was actually returned by the remote server.
net::RedirectInfo CreateRedirectInfo(
    const network::ResourceRequest& original_request,
    const GURL& new_url,
    int response_code,
    const std::optional<std::string>& referrer_policy_header) {
  return net::RedirectInfo::ComputeRedirectInfo(
      original_request.method, original_request.url,
      original_request.site_for_cookies,
      original_request.update_first_party_url_on_redirect
          ? net::RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT
          : net::RedirectInfo::FirstPartyURLPolicy::NEVER_CHANGE_URL,
      original_request.referrer_policy, original_request.referrer.spec(),
      response_code, new_url, referrer_policy_header,
      /*insecure_scheme_was_upgraded=*/false, /*copy_fragment=*/false,
      /*is_signed_exchange_fallback_redirect=*/false);
}

}  // namespace

UrlRequestProxyingURLLoaderFactory::InProgressRequest::InProgressRequest(
    UrlRequestProxyingURLLoaderFactory* factory,
    uint64_t request_id,
    int32_t network_service_request_id,
    int32_t view_routing_id,
    int32_t frame_routing_id,
    uint32_t options,
    ukm::SourceIdObj ukm_source_id,
    const network::ResourceRequest& request,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner)
    : factory_(factory),
      request_(request),
      original_initiator_(request.request_initiator),
      request_id_(request_id),
      network_service_request_id_(network_service_request_id),
      view_routing_id_(view_routing_id),
      frame_routing_id_(frame_routing_id),
      options_(options),
      ukm_source_id_(ukm_source_id),
      traffic_annotation_(traffic_annotation),
      proxied_loader_receiver_(this,
                               std::move(loader_receiver),
                               navigation_response_task_runner),
      target_client_(std::move(client)),
      current_response_(network::mojom::URLResponseHead::New()),
      navigation_response_task_runner_(navigation_response_task_runner) {
  TRACE_EVENT_WITH_FLOW1(
      "extensions",
      "UrlRequestProxyingURLLoaderFactory::InProgressRequest::"
      "InProgressRequest",
      TRACE_ID_WITH_SCOPE(kUrlRequestProxyingURLLoaderFactoryScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_OUT, "url", request.url.spec());

  // If there is a client error, clean up the request.
  target_client_.set_disconnect_handler(
      base::BindOnce(&UrlRequestProxyingURLLoaderFactory::InProgressRequest::
                         OnClientDisconnected,
                     weak_factory_.GetWeakPtr()));
  proxied_loader_receiver_.set_disconnect_with_reason_handler(
      base::BindOnce(&UrlRequestProxyingURLLoaderFactory::InProgressRequest::
                         OnLoaderDisconnected,
                     weak_factory_.GetWeakPtr()));
}

UrlRequestProxyingURLLoaderFactory::InProgressRequest::InProgressRequest(
    UrlRequestProxyingURLLoaderFactory* factory,
    uint64_t request_id,
    int32_t frame_routing_id,
    const network::ResourceRequest& request)
    : factory_(factory),
      request_(request),
      original_initiator_(request.request_initiator),
      request_id_(request_id),
      frame_routing_id_(frame_routing_id),
      ukm_source_id_(ukm::kInvalidSourceIdObj),
      proxied_loader_receiver_(this),
      for_cors_preflight_(true) // ,
      {
  TRACE_EVENT_WITH_FLOW1(
      "extensions",
      "UrlRequestProxyingURLLoaderFactory::InProgressRequest::"
      "InProgressRequest",
      TRACE_ID_WITH_SCOPE(kUrlRequestProxyingURLLoaderFactoryScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_OUT, "url", request.url.spec());
}

UrlRequestProxyingURLLoaderFactory::InProgressRequest::~InProgressRequest() {
  DCHECK_NE(state_, State::kInvalid);

  TRACE_EVENT_WITH_FLOW1(
      "extensions",
      "UrlRequestProxyingURLLoaderFactory::InProgressRequest::"
      "~InProgressRequest",
      TRACE_ID_WITH_SCOPE(kUrlRequestProxyingURLLoaderFactoryScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN, "state", state_);

  if (request_.keepalive && !for_cors_preflight_) {
    UMA_HISTOGRAM_ENUMERATION("Extensions.WebRequest.KeepaliveRequestState",
                              state_);
    if (base::FeatureList::IsEnabled(
            extensions_features::kReportKeepaliveUkm)) {
      ukm::builders::Extensions_WebRequest_KeepaliveRequestFinished(
          ukm_source_id_)
          .SetState(state_)
          .SetNumRedirects(num_redirects_)
          .Record(ukm::UkmRecorder::Get());
    }
  }
  // This is important to ensure that no outstanding blocking requests continue
  // to reference state owned by this object.
  if (info_) {
    UrlRequestEventRouter::Get(factory_->browser_context_)
        ->OnRequestWillBeDestroyed(factory_->browser_context_, &info_.value());
  }
  if (on_before_send_headers_callback_) {
    std::move(on_before_send_headers_callback_)
        .Run(net::ERR_ABORTED, std::nullopt);
  }
  if (on_headers_received_callback_) {
    std::move(on_headers_received_callback_)
        .Run(net::ERR_ABORTED, std::nullopt, std::nullopt);
  }
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::Restart() {
  UpdateRequestInfo();
  RestartInternal();
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::
    UpdateRequestInfo() {
  // Derive a new WebRequestInfo value any time |Restart()| is called, because
  // the details in |request_| may have changed e.g. if we've been redirected.
  // |request_initiator| can be modified on redirects, but we keep the original
  // for |initiator| in the event. See also
  // https://developer.chrome.com/extensions/webRequest#event-onBeforeRequest.
  // TODO (gonzazoid) write actual documentation
  network::ResourceRequest request_for_info = request_;
  request_for_info.request_initiator = original_initiator_;
  info_.emplace(UrlRequestInfoInitParams(
      request_id_, factory_->render_process_id_, frame_routing_id_,
      factory_->navigation_ui_data_ ? factory_->navigation_ui_data_->DeepCopy()
                                    : nullptr,
      request_for_info, factory_->IsForDownload(),
      !(options_ & network::mojom::kURLLoadOptionSynchronous),
      factory_->IsForServiceWorkerScript(), factory_->navigation_id_));
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::RestartInternal() {
  DCHECK_EQ(info_->url, request_.url)
      << "UpdateRequestInfo must have been called first";
  is_header_client_receiver_paused_ = false;
  // If the header client will be used, we start the request immediately, and
  // OnBeforeSendHeaders and OnSendHeaders will be handled there. Otherwise,
  // send these events before the request starts.
  base::RepeatingCallback<void(int)> continuation;
  const auto state_on_error = State::kRejectedByOnBeforeRequest;
  if (for_cors_preflight_) {
    // TODO (gonzazoid) we gonna weight in here!
    // In this case we do nothing because extensions should see nothing.
    return;
  } else {
    continuation =
        base::BindRepeating(&InProgressRequest::ContinueToBeforeSendHeaders,
                            weak_factory_.GetWeakPtr(), state_on_error);
  }

  redirect_url_ = GURL();
  response_ = extension_url_request_api_helpers::BlockingResponse();
  bool should_collapse_initiator = false;
  int result = UrlRequestEventRouter::Get(factory_->browser_context_)
                   ->OnBeforeRequest(factory_->browser_context_, &info_.value(),
                                     continuation, &redirect_url_, &response_,
                                     &should_collapse_initiator);
  if (result == net::ERR_BLOCKED_BY_CLIENT) {
    // The request was cancelled synchronously. Dispatch an error notification
    // and terminate the request.
    network::URLLoaderCompletionStatus status =
        CreateURLLoaderCompletionStatus(result, should_collapse_initiator);
    OnRequestError(status, state_on_error);
    return;
  }

  if (result == net::ERR_IO_PENDING) {
    // One or more listeners is blocking, so the request must be paused until
    // they respond. |continuation| above will be invoked asynchronously to
    // continue or cancel the request.
    //
    // We pause the receiver here to prevent further client message processing.
    if (proxied_client_receiver_.is_bound())
      proxied_client_receiver_.Pause();

    // Pause the header client, since we want to wait until OnBeforeRequest has
    // finished before processing any future events.
    if (header_client_receiver_.is_bound()) {
      header_client_receiver_.Pause();
      is_header_client_receiver_paused_ = true;
    }
    return;
  }
  DCHECK_EQ(net::OK, result);

  continuation.Run(net::OK);
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const std::optional<GURL>& new_url) {
  if (new_url)
    request_.url = new_url.value();

  // Call this before checking |current_request_uses_header_client_| as it
  // calculates it.
  UpdateRequestInfo();

  ++num_redirects_;
  RestartInternal();
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::SetPriority(
    net::RequestPriority priority,
    int32_t intra_priority_value) {
  if (target_loader_.is_bound())
    target_loader_->SetPriority(priority, intra_priority_value);
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::
    PauseReadingBodyFromNet() {
  if (target_loader_.is_bound())
    target_loader_->PauseReadingBodyFromNet();
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::
    ResumeReadingBodyFromNet() {
  if (target_loader_.is_bound())
    target_loader_->ResumeReadingBodyFromNet();
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::OnReceiveEarlyHints(
    network::mojom::EarlyHintsPtr early_hints) {
  target_client_->OnReceiveEarlyHints(std::move(early_hints));
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::OnReceiveResponse(
    network::mojom::URLResponseHeadPtr head,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  TRACE_EVENT_WITH_FLOW0(
      "extensions",
      "UrlRequestProxyingURLLoaderFactory::InProgressRequest::"
      "OnReceiveResponse",
      TRACE_ID_WITH_SCOPE(kUrlRequestProxyingURLLoaderFactoryScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  current_body_ = std::move(body);
  current_cached_metadata_ = std::move(cached_metadata);

  current_response_ = std::move(head);
  HandleResponseOrRedirectHeaders(base::BindOnce(
      &InProgressRequest::OverwriteHeadersAndContinueToResponseStarted,
      weak_factory_.GetWeakPtr()));
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  TRACE_EVENT_WITH_FLOW0(
      "extensions",
      "UrlRequestProxyingURLLoaderFactory::InProgressRequest::"
      "OnReceiveRedirect",
      TRACE_ID_WITH_SCOPE(kUrlRequestProxyingURLLoaderFactoryScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  current_response_ = std::move(head);
  HandleResponseOrRedirectHeaders(
      base::BindOnce(&InProgressRequest::ContinueToBeforeRedirect,
                     weak_factory_.GetWeakPtr(), redirect_info));
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {
  target_client_->OnUploadProgress(current_position, total_size,
                                   std::move(callback));
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::
    OnTransferSizeUpdated(int32_t transfer_size_diff) {
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kWebRequestProxyingURLLoaderFactory);

  target_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  TRACE_EVENT_WITH_FLOW2(
      "extensions",
      "UrlRequestProxyingURLLoaderFactory::InProgressRequest::"
      "OnComplete",
      TRACE_ID_WITH_SCOPE(kUrlRequestProxyingURLLoaderFactoryScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "error_code",
      status.error_code, "extended_error_code", status.extended_error_code);

  if (status.error_code != net::OK) {
    OnNetworkError(status);
    return;
  }

  state_ = kCompleted;
  target_client_->OnComplete(status);

  // Deletes |this|.
  factory_->RemoveRequest(network_service_request_id_, request_id_);
}

bool UrlRequestProxyingURLLoaderFactory::IsForServiceWorkerScript() const {
  return loader_factory_type_ == content::ContentBrowserClient::
                                     URLLoaderFactoryType::kServiceWorkerScript;
}

bool UrlRequestProxyingURLLoaderFactory::IsForDownload() const {
  return loader_factory_type_ ==
         content::ContentBrowserClient::URLLoaderFactoryType::kDownload;
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::OnLoaderCreated(
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
  TRACE_EVENT_WITH_FLOW1(
      "extensions",
      "UrlRequestProxyingURLLoaderFactory::InProgressRequest::"
      "OnLoaderCreated",
      TRACE_ID_WITH_SCOPE(kUrlRequestProxyingURLLoaderFactoryScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
      "for_cors_preflight", for_cors_preflight_);

  // When CORS is involved there may be multiple network::URLLoader associated
  // with this InProgressRequest, because CorsURLLoader may create a new
  // network::URLLoader for the same request id in redirect handling - see
  // CorsURLLoader::FollowRedirect. In such a case the old network::URLLoader
  // is going to be detached fairly soon, so we don't need to take care of it.
  // We need this explicit reset to avoid a DCHECK failure in mojo::Receiver.
  header_client_receiver_.reset();

  header_client_receiver_.Bind(std::move(receiver));
  if (is_header_client_receiver_paused_)
    header_client_receiver_.Pause();
  if (for_cors_preflight_) {
    // In this case we don't have |target_loader_| and
    // |proxied_client_receiver_|, and |receiver| is the only connection to the
    // network service, so we observe mojo connection errors.
    header_client_receiver_.set_disconnect_handler(base::BindOnce(
        &UrlRequestProxyingURLLoaderFactory::InProgressRequest::OnNetworkError,
        weak_factory_.GetWeakPtr(),
        CreateURLLoaderCompletionStatus(net::ERR_FAILED)));
  }
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::OnBeforeSendHeaders(
    const net::HttpRequestHeaders& headers,
    OnBeforeSendHeadersCallback callback) {
  TRACE_EVENT_WITH_FLOW0(
      "extensions",
      "UrlRequestProxyingURLLoaderFactory::InProgressRequest::"
      "OnBeforeSendHeaders",
      TRACE_ID_WITH_SCOPE(kUrlRequestProxyingURLLoaderFactoryScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

    std::move(callback).Run(net::OK, std::nullopt);
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::OnHeadersReceived(
    const std::string& headers,
    const net::IPEndPoint& remote_endpoint,
    OnHeadersReceivedCallback callback) {
  TRACE_EVENT_WITH_FLOW1(
      "extensions",
      "UrlRequestProxyingURLLoaderFactory::InProgressRequest::"
      "OnHeadersReceived",
      TRACE_ID_WITH_SCOPE(kUrlRequestProxyingURLLoaderFactoryScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
      "for_cors_preflight", for_cors_preflight_);

  std::move(callback).Run(net::OK, std::nullopt, std::nullopt);

  if (for_cors_preflight_) {
    // CORS preflight is supported only when "extraHeaders" is specified.
    // Deletes |this|.
    factory_->RemoveRequest(network_service_request_id_, request_id_);
  }
  return;
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::
    HandleBeforeRequestRedirect() {
  TRACE_EVENT_WITH_FLOW0(
      "extensions",
      "UrlRequestProxyingURLLoaderFactory::InProgressRequest::"
      "HandleBeforeRequestRedirect",
      TRACE_ID_WITH_SCOPE(kUrlRequestProxyingURLLoaderFactoryScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  // The extension requested a redirect. Close the connection with the current
  // URLLoader and inform the URLLoaderClient the WebRequest API generated a
  // redirect. To load |redirect_url_|, a new URLLoader will be recreated
  // after receiving FollowRedirect().

  // Forgetting to close the connection with the current URLLoader caused
  // bugs. The latter doesn't know anything about the redirect. Continuing
  // the load with it gives unexpected results. See
  // https://crbug.com/882661#c72.
  proxied_client_receiver_.reset();
  header_client_receiver_.reset();
  target_loader_.reset();

  constexpr int kInternalRedirectStatusCode = net::HTTP_TEMPORARY_REDIRECT;

  net::RedirectInfo redirect_info =
      CreateRedirectInfo(request_, redirect_url_, kInternalRedirectStatusCode,
                         /*referrer_policy_header=*/std::nullopt);

  auto head = network::mojom::URLResponseHead::New();
  std::string headers = base::StringPrintf(
      "HTTP/1.1 %i Internal Redirect\n"
      "Location: %s\n"
      "Non-Authoritative-Reason: WebRequest API\n\n",
      kInternalRedirectStatusCode, redirect_url_.spec().c_str());

  // Cross-origin requests need to modify the Origin header to 'null'. Since
  // CorsURLLoader sets |request_initiator| to the Origin request header in
  // NetworkService, we need to modify |request_initiator| here to craft the
  // Origin header indirectly.
  // Following checks implement the step 10 of "4.4. HTTP-redirect fetch",
  // https://fetch.spec.whatwg.org/#http-redirect-fetch
  if (request_.request_initiator &&
      (!url::IsSameOriginWith(redirect_url_, request_.url) &&
       !request_.request_initiator->IsSameOriginWith(request_.url))) {
    // Reset the initiator to pretend tainted origin flag of the spec is set.
    request_.request_initiator = url::Origin();
  }
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  head->encoded_data_length = 0;

  current_response_ = std::move(head);
  ContinueToBeforeRedirect(redirect_info, net::OK);
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::
    HandleResponse() {
  TRACE_EVENT_WITH_FLOW0(
      "extensions",
      "UrlRequestProxyingURLLoaderFactory::InProgressRequest::"
      "HandleResponse",
      TRACE_ID_WITH_SCOPE(kUrlRequestProxyingURLLoaderFactoryScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);


  auto head = network::mojom::URLResponseHead::New();
  head->did_mime_sniff = true;

  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>("HTTP/1.1 " + response_.status + " " + response_.status_text + "\n\n");

  for(const auto& header : response_.headers) {
    head->headers->AddHeader(header.first, header.second); // TODO Add/SetHeader
  }

  std::string mime_type;
  std::string charset;
  head->headers->GetMimeTypeAndCharset(&mime_type, &charset);
  head->mime_type = mime_type;
  head->charset = charset;

  mojo::ScopedDataPipeProducerHandle producer_handle;
  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  if (CreateDataPipe(nullptr, producer_handle, consumer_handle) !=
      MOJO_RESULT_OK) {
    // CommitCompleted(net::ERR_INSUFFICIENT_RESOURCES,
    //                 "Can't create empty data pipe");
    // TODO !!!
    return;
  }
  size_t bytes_written = 0;
  producer_handle->WriteData(base::as_byte_span(response_.body), MOJO_WRITE_DATA_FLAG_NONE, bytes_written);
  // producer_handle.reset();

  target_client_->OnReceiveResponse(
     std::move(head),
     /*body=*/ std::move(consumer_handle), std::nullopt);
  target_client_->OnComplete(network::URLLoaderCompletionStatus(net::OK));

  // Deletes |this|.
  // factory_->RemoveRequest(network_service_request_id_, request_id_);
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::
    ContinueToBeforeSendHeaders(State state_on_error, int error_code) {
  TRACE_EVENT_WITH_FLOW2(
      "extensions",
      "UrlRequestProxyingURLLoaderFactory::InProgressRequest::"
      "ContinueToBeforeSendHeaders",
      TRACE_ID_WITH_SCOPE(kUrlRequestProxyingURLLoaderFactoryScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "state_on_error",
      state_on_error, "error_code", error_code);

  if (error_code != net::OK) {
    OnRequestError(CreateURLLoaderCompletionStatus(error_code), state_on_error);
    return;
  }

  if (!response_.empty()) {
    HandleResponse();
    return;
  }

  if (/* !current_request_uses_header_client_ && */ !redirect_url_.is_empty()) {
    HandleBeforeRequestRedirect();
    return;
  }

  if (proxied_client_receiver_.is_bound())
    proxied_client_receiver_.Resume();

  if (request_.url.SchemeIsHTTPOrHTTPS() ||
      request_.url.SchemeIs(url::kUuidInPackageScheme)) {
    // NOTE: While it does not appear to be documented (and in fact it may be
    // intuitive), |onBeforeSendHeaders| is only dispatched for HTTP and HTTPS
    // and urn: requests.

    state_on_error = State::kRejectedByOnBeforeSendHeaders;
    auto continuation =
        base::BindRepeating(&InProgressRequest::ContinueToSendHeaders,
                            weak_factory_.GetWeakPtr(), state_on_error);
  }

  ContinueToSendHeadersWithOk(std::set<std::string>(), std::set<std::string>());
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::
    ContinueToBeforeSendHeadersWithOk() {
  ContinueToBeforeSendHeaders(State::kInvalid, net::OK);
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::
    ContinueToStartRequest(State state_on_error, int error_code) {
  TRACE_EVENT_WITH_FLOW2(
      "extensions",
      "UrlRequestProxyingURLLoaderFactory::InProgressRequest::"
      "ContinueToStartRequest",
      TRACE_ID_WITH_SCOPE(kUrlRequestProxyingURLLoaderFactoryScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "state_on_error",
      state_on_error, "error_code", error_code);

  if (error_code != net::OK) {
    OnRequestError(CreateURLLoaderCompletionStatus(error_code), state_on_error);
    return;
  }

  if (proxied_client_receiver_.is_bound())
    proxied_client_receiver_.Resume();

  if (header_client_receiver_.is_bound()) {
    header_client_receiver_.Resume();
    is_header_client_receiver_paused_ = false;
  }

  if (for_cors_preflight_) {
    // For CORS preflight requests, we have already started the request in
    // the network service. We did block the request by blocking
    // |header_client_receiver_|, which we unblocked right above.
    return;
  }

  if (!target_loader_.is_bound() && factory_->target_factory_.is_bound()) {
    // No extensions have cancelled us up to this point, so it's now OK to
    // initiate the real network request.
    uint32_t options = options_;
    // Even if this request does not use the header client, future redirects
    // might, so we need to set the option on the loader.
    factory_->target_factory_->CreateLoaderAndStart(
        target_loader_.BindNewPipeAndPassReceiver(
            navigation_response_task_runner_),
        network_service_request_id_, options, request_,
        proxied_client_receiver_.BindNewPipeAndPassRemote(
            navigation_response_task_runner_),
        traffic_annotation_);
  }

  // From here the lifecycle of this request is driven by subsequent events on
  // either |proxied_loader_receiver_|, |proxied_client_receiver_|, or
  // |header_client_receiver_|.
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::
    ContinueToStartRequestWithOk() {
  ContinueToStartRequest(State::kInvalid, net::OK);
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::
    ContinueToSendHeaders(State state_on_error,
                          const std::set<std::string>& removed_headers,
                          const std::set<std::string>& set_headers,
                          int error_code) {
  TRACE_EVENT_WITH_FLOW2(
      "extensions",
      "UrlRequestProxyingURLLoaderFactory::InProgressRequest::"
      "ContinueToSendHeaders",
      TRACE_ID_WITH_SCOPE(kUrlRequestProxyingURLLoaderFactoryScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "state_on_error",
      state_on_error, "error_code", error_code);

  if (error_code != net::OK) {
    OnRequestError(CreateURLLoaderCompletionStatus(error_code), state_on_error);
    return;
  }

  if (proxied_client_receiver_.is_bound())
    proxied_client_receiver_.Resume();

  if (request_.url.SchemeIsHTTPOrHTTPS() ||
      request_.url.SchemeIs(url::kUuidInPackageScheme)) {
    // NOTE: While it does not appear to be documented (and in fact it may be
    // intuitive), |onSendHeaders| is only dispatched for HTTP and HTTPS
    // and urn: requests.
    // WebRequestEventRouter::Get(factory_->browser_context_)
    //     ->OnSendHeaders(factory_->browser_context_, &info_.value(),
    //                     request_.headers);
  }

  ContinueToStartRequestWithOk();
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::
    ContinueToSendHeadersWithOk(const std::set<std::string>& removed_headers,
                                const std::set<std::string>& set_headers) {
  ContinueToSendHeaders(State::kInvalid, removed_headers, set_headers, net::OK);
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::
    ContinueToHandleOverrideHeaders(int error_code) {
  TRACE_EVENT_WITH_FLOW1(
      "extensions",
      "UrlRequestProxyingURLLoaderFactory::InProgressRequest::"
      "ContinueToHandleOverrideHeaders",
      TRACE_ID_WITH_SCOPE(kUrlRequestProxyingURLLoaderFactoryScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "error_code",
      error_code);

  if (error_code != net::OK) {
    const int status_code = current_response_->headers
                                ? current_response_->headers->response_code()
                                : 0;
    State state;
    if (status_code == net::HTTP_UNAUTHORIZED) {
      state = State::kRejectedByOnHeadersReceivedForAuth;
    } else if (net::HttpResponseHeaders::IsRedirectResponseCode(status_code)) {
      state = State::kRejectedByOnHeadersReceivedForRedirect;
    } else {
      state = State::kRejectedByOnHeadersReceivedForFinalResponse;
    }
    OnRequestError(CreateURLLoaderCompletionStatus(error_code), state);
    return;
  }

  DCHECK(on_headers_received_callback_);
  std::optional<std::string> headers;

  if (for_cors_preflight_ && !redirect_url_.is_empty()) {
    OnRequestError(CreateURLLoaderCompletionStatus(net::ERR_FAILED),
                   State::kRejectedByOnHeadersReceivedForRedirect);
    return;
  }

  std::move(on_headers_received_callback_).Run(net::OK, headers, redirect_url_);
  // override_headers_ = nullptr;

  if (for_cors_preflight_) {
    // If this is for CORS preflight, there is no associated client.
    // info_->AddResponseInfoFromResourceResponse(*current_response_);
    // Do not finish proxied preflight requests that require proxy auth.
    // The request is not finished yet, give control back to network service
    // which will start authentication process.
    // if (info_->response_code == net::HTTP_PROXY_AUTHENTICATION_REQUIRED)
    //   return;
    // We notify the completion here, and delete |this|.
    // WebRequestEventRouter::Get(factory_->browser_context_)
    //     ->OnResponseStarted(factory_->browser_context_, &info_.value(),
    //                         net::OK);
    // WebRequestEventRouter::Get(factory_->browser_context_)
     //    ->OnCompleted(factory_->browser_context_, &info_.value(), net::OK);

    factory_->RemoveRequest(network_service_request_id_, request_id_);
    return;
  }

  if (proxied_client_receiver_.is_bound())
    proxied_client_receiver_.Resume();
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::
    OverwriteHeadersAndContinueToResponseStarted(int error_code) {
  DCHECK(!for_cors_preflight_);

  TRACE_EVENT_WITH_FLOW2(
      "extensions",
      "UrlRequestProxyingURLLoaderFactory::InProgressRequest::"
      "OverwriteHeadersAndContinueToResponseStarted",
      TRACE_ID_WITH_SCOPE(kUrlRequestProxyingURLLoaderFactoryScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "error_code",
      error_code, "loader_factory_type", factory_->loader_factory_type());

  if (error_code != net::OK) {
    OnRequestError(CreateURLLoaderCompletionStatus(error_code),
                   State::kRejectedByOnHeadersReceivedForFinalResponse);
    return;
  }

  ContinueToResponseStarted();
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::
    AssignParsedHeadersAndContinueToResponseStarted(
        network::mojom::ParsedHeadersPtr parsed_headers) {
  TRACE_EVENT_WITH_FLOW0(
      "extensions",
      "UrlRequestProxyingURLLoaderFactory::InProgressRequest::"
      "AssignParsedHeadersAndContinueToResponseStarted",
      TRACE_ID_WITH_SCOPE(kUrlRequestProxyingURLLoaderFactoryScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  current_response_->parsed_headers = std::move(parsed_headers);
  ContinueToResponseStarted();
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::
    ContinueToResponseStarted() {
  TRACE_EVENT_WITH_FLOW0(
      "extensions",
      "UrlRequestProxyingURLLoaderFactory::InProgressRequest::"
      "ContinueToResponseStarted",
      TRACE_ID_WITH_SCOPE(kUrlRequestProxyingURLLoaderFactoryScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  if (state_ == State::kInProgress) {
    state_ = State::kInProgressWithFinalResponseReceived;
  }

  info_->AddResponseInfoFromResourceResponse(*current_response_);

  proxied_client_receiver_.Resume();

  target_client_->OnReceiveResponse(current_response_.Clone(),
                                    std::move(current_body_),
                                    std::move(current_cached_metadata_));
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::
    ContinueToBeforeRedirect(const net::RedirectInfo& redirect_info,
                             int error_code) {
  TRACE_EVENT_WITH_FLOW1(
      "extensions",
      "UrlRequestProxyingURLLoaderFactory::InProgressRequest::"
      "ContinueToBeforeRedirect",
      TRACE_ID_WITH_SCOPE(kUrlRequestProxyingURLLoaderFactoryScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "error_code",
      error_code);

  if (error_code != net::OK) {
    OnRequestError(CreateURLLoaderCompletionStatus(error_code),
                   kRejectedByOnHeadersReceivedForRedirect);
    return;
  }

  info_->AddResponseInfoFromResourceResponse(*current_response_);

  if (proxied_client_receiver_.is_bound())
    proxied_client_receiver_.Resume();

  UrlRequestEventRouter::Get(factory_->browser_context_)
      ->OnBeforeRedirect(factory_->browser_context_, &info_.value(),
                         redirect_info.new_url);
  target_client_->OnReceiveRedirect(redirect_info, current_response_.Clone());
  request_.url = redirect_info.new_url;
  request_.method = redirect_info.new_method;
  request_.site_for_cookies = redirect_info.new_site_for_cookies;
  request_.referrer = GURL(redirect_info.new_referrer);
  request_.referrer_policy = redirect_info.new_referrer_policy;
  if (request_.trusted_params) {
    request_.trusted_params->isolation_info =
        request_.trusted_params->isolation_info.CreateForRedirect(
            url::Origin::Create(redirect_info.new_url));
  }

  // The request method can be changed to "GET". In this case we need to
  // reset the request body manually.
  if (request_.method == net::HttpRequestHeaders::kGetMethod)
    request_.request_body = nullptr;
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::
    HandleResponseOrRedirectHeaders(net::CompletionOnceCallback continuation) {
  TRACE_EVENT_WITH_FLOW0(
      "extensions",
      "UrlRequestProxyingURLLoaderFactory::InProgressRequest::"
      "HandleResponseOrRedirectHeaders",
      TRACE_ID_WITH_SCOPE(kUrlRequestProxyingURLLoaderFactoryScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  // override_headers_ = nullptr;
  redirect_url_ = GURL();

  auto callback_pair = base::SplitOnceCallback(std::move(continuation));
  if (request_.url.SchemeIsHTTPOrHTTPS() ||
      request_.url.SchemeIs(url::kUuidInPackageScheme)) {
    DCHECK(info_.has_value());
  }

  std::move(callback_pair.second).Run(net::OK);
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::OnRequestError(
    const network::URLLoaderCompletionStatus& status,
    State state) {
  TRACE_EVENT_WITH_FLOW2(
      "extensions",
      "UrlRequestProxyingURLLoaderFactory::InProgressRequest::"
      "OnRequestError",
      TRACE_ID_WITH_SCOPE(kUrlRequestProxyingURLLoaderFactoryScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "error_code",
      status.error_code, "state", state);

  if (target_client_) {
    target_client_->OnComplete(status);
  }
  state_ = state;

  // Deletes |this|.
  factory_->RemoveRequest(network_service_request_id_, request_id_);
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::OnNetworkError(
    const network::URLLoaderCompletionStatus& status) {
  TRACE_EVENT_WITH_FLOW2(
      "extensions",
      "UrlRequestProxyingURLLoaderFactory::InProgressRequest::"
      "OnNetworkError",
      TRACE_ID_WITH_SCOPE(kUrlRequestProxyingURLLoaderFactoryScope,
                          TRACE_ID_LOCAL(request_id_)),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "error_code",
      status.error_code, "state", state_);

  State state = state_;
  if (state_ == State::kInProgress) {
    state = State::kRejectedByNetworkError;
  } else if (state_ == State::kInProgressWithFinalResponseReceived) {
    state = State::kRejectedByNetworkErrorAfterReceivingFinalResponse;
  }
  OnRequestError(status, state);
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::
    OnClientDisconnected() {
  State state = state_;
  if (state_ == State::kInProgress) {
    state = State::kDetachedFromClient;
  } else if (state_ == State::kInProgressWithFinalResponseReceived) {
    state = State::kDetachedFromClientAfterReceivingResponse;
  }
  OnRequestError(CreateURLLoaderCompletionStatus(net::ERR_ABORTED), state);
}

void UrlRequestProxyingURLLoaderFactory::InProgressRequest::
    OnLoaderDisconnected(uint32_t custom_reason,
                         const std::string& description) {
  if (custom_reason == network::mojom::URLLoader::kClientDisconnectReason &&
      description == blink::ThrottlingURLLoader::kFollowRedirectReason) {
    // Save the ID here because this request will be restarted with a new
    // URLLoader instead of continuing with FollowRedirect(). The saved ID will
    // be retrieved in the restarted request, which will call
    // RequestIDGenerator::Generate() with the same ID pair.
    factory_->request_id_generator_->SaveID(
        view_routing_id_, network_service_request_id_, request_id_);

    state_ = State::kRedirectFollowedByAnotherInProgressRequest;
    // Deletes |this|.
    factory_->RemoveRequest(network_service_request_id_, request_id_);
  } else {
    OnNetworkError(CreateURLLoaderCompletionStatus(net::ERR_ABORTED));
  }
}

// Determines whether it is safe to redirect from |from_url| to |to_url|.
bool UrlRequestProxyingURLLoaderFactory::InProgressRequest::IsRedirectSafe(
    const GURL& upstream_url,
    const GURL& target_url,
    bool is_navigation_request) {
  // For navigations, non-web accessible resources will be blocked by
  // ExtensionNavigationThrottle.
  if (!is_navigation_request &&
      target_url.SchemeIs(extensions::kExtensionScheme)) {
    const Extension* extension =
        ExtensionRegistry::Get(factory_->browser_context_)
            ->enabled_extensions()
            .GetByID(target_url.host());
    if (!extension) {
      return false;
    }
    return WebAccessibleResourcesInfo::IsResourceWebAccessibleRedirect(
        extension, target_url, original_initiator_, upstream_url);
  }
  return content::IsSafeRedirectTarget(upstream_url, target_url);
}

network::URLLoaderCompletionStatus UrlRequestProxyingURLLoaderFactory::
    InProgressRequest::CreateURLLoaderCompletionStatus(
        int error_code,
        bool collapse_initiator) {
  network::URLLoaderCompletionStatus status(error_code);
  status.should_collapse_initiator = collapse_initiator;
  if (error_code == net::ERR_BLOCKED_BY_CLIENT) {
    status.extended_error_code =
        ExtensionsClient::Get()->GetExtensionExtendedErrorCode().value_or(0);
  }

  return status;
}

UrlRequestProxyingURLLoaderFactory::UrlRequestProxyingURLLoaderFactory(
    content::BrowserContext* browser_context,
    int render_process_id,
    int frame_routing_id,
    int view_routing_id,
    UrlRequestAPI::RequestIDGenerator* request_id_generator,
    std::unique_ptr<ExtensionNavigationUIData> navigation_ui_data,
    std::optional<int64_t> navigation_id,
    ukm::SourceIdObj ukm_source_id,
    network::URLLoaderFactoryBuilder& factory_builder,
    mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
        header_client_receiver,
    UrlRequestAPI::ProxySet* proxies,
    content::ContentBrowserClient::URLLoaderFactoryType loader_factory_type,
    scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner)
    : browser_context_(browser_context),
      render_process_id_(render_process_id),
      frame_routing_id_(frame_routing_id),
      view_routing_id_(view_routing_id),
      request_id_generator_(request_id_generator),
      navigation_ui_data_(std::move(navigation_ui_data)),
      navigation_id_(std::move(navigation_id)),
      proxies_(proxies),
      loader_factory_type_(loader_factory_type),
      ukm_source_id_(ukm_source_id),
      navigation_response_task_runner_(
          std::move(navigation_response_task_runner)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // base::Unretained is safe here because the callback will be
  // canceled when |shutdown_notifier_subscription_| is destroyed, and
  // |proxies_| owns this.
  shutdown_notifier_subscription_ =
      ShutdownNotifierFactory::GetInstance()
          ->Get(browser_context)
          ->Subscribe(base::BindRepeating(&UrlRequestAPI::ProxySet::RemoveProxy,
                                          base::Unretained(proxies_), this));

  auto [loader_receiver, target_factory_remote] = factory_builder.Append();

  target_factory_.Bind(std::move(target_factory_remote));
  target_factory_.set_disconnect_handler(
      base::BindOnce(&UrlRequestProxyingURLLoaderFactory::OnTargetFactoryError,
                     base::Unretained(this)));

  proxy_receivers_.Add(this, std::move(loader_receiver),
                       navigation_response_task_runner_);
  proxy_receivers_.set_disconnect_handler(base::BindRepeating(
      &UrlRequestProxyingURLLoaderFactory::OnProxyBindingError,
      base::Unretained(this)));

  if (header_client_receiver) {
    url_loader_header_client_receiver_.Bind(std::move(header_client_receiver),
                                            navigation_response_task_runner_);
  }
}

void UrlRequestProxyingURLLoaderFactory::StartProxying(
    content::BrowserContext* browser_context,
    int render_process_id,
    int frame_routing_id,
    int view_routing_id,
    UrlRequestAPI::RequestIDGenerator* request_id_generator,
    std::unique_ptr<ExtensionNavigationUIData> navigation_ui_data,
    std::optional<int64_t> navigation_id,
    ukm::SourceIdObj ukm_source_id,
    network::URLLoaderFactoryBuilder& factory_builder,
    mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
        header_client_receiver,
    UrlRequestAPI::ProxySet* proxies,
    content::ContentBrowserClient::URLLoaderFactoryType loader_factory_type,
    scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto proxy = std::make_unique<UrlRequestProxyingURLLoaderFactory>(
      browser_context, render_process_id, frame_routing_id, view_routing_id,
      request_id_generator, std::move(navigation_ui_data),
      std::move(navigation_id), ukm_source_id, factory_builder,
      std::move(header_client_receiver), proxies, loader_factory_type,
      std::move(navigation_response_task_runner));

  proxies->AddProxy(std::move(proxy));
}

void UrlRequestProxyingURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Make sure we are not proxying a browser initiated non-navigation
  // request except for loading service worker scripts.
  DCHECK(render_process_id_ != -1 || navigation_ui_data_ ||
         IsForServiceWorkerScript());

  // The |url_request_id| doesn't really matter. It just needs to be
  // unique per-BrowserContext so extensions can make sense of it.
  // Note that |network_service_request_id_| by contrast is not
  // necessarily unique, so we don't use it for identity here. This
  // request ID may be the same as a previous request if the previous
  // request was redirected to a URL that required a different loader.
  const uint64_t url_request_id =
      request_id_generator_->Generate(view_routing_id_, request_id);

  if (request_id) {
    // Only requests with a non-zero request ID can have their proxy
    // associated with said ID. This is necessary to support
    // correlation against any auth events received by the browser.
    // Requests with a request ID of 0 therefore do not support
    // dispatching |WebRequest.onAuthRequired| events.
    proxies_->AssociateProxyWithRequestId(
        this, content::GlobalRequestID(render_process_id_, request_id));
    network_request_id_to_url_request_id_.emplace(request_id, url_request_id);
  }

  auto result = requests_.emplace(
      url_request_id, std::make_unique<InProgressRequest>(
                          this, url_request_id, request_id, view_routing_id_,
                          frame_routing_id_, options, ukm_source_id_, request,
                          traffic_annotation, std::move(loader_receiver),
                          std::move(client), navigation_response_task_runner_));
  result.first->second->Restart();
}

void UrlRequestProxyingURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  proxy_receivers_.Add(this, std::move(loader_receiver));
}

void UrlRequestProxyingURLLoaderFactory::OnLoaderCreated(
    int32_t request_id,
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
  auto it = network_request_id_to_url_request_id_.find(request_id);
  if (it == network_request_id_to_url_request_id_.end())
    return;

  auto request_it = requests_.find(it->second);
  CHECK(request_it != requests_.end(), base::NotFatalUntil::M130);
  request_it->second->OnLoaderCreated(std::move(receiver));
}

void UrlRequestProxyingURLLoaderFactory::OnLoaderForCorsPreflightCreated(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
  // Please note that the URLLoader is now starting, without waiting for
  // additional signals from here. The URLLoader will be blocked before
  // sending HTTP request headers (TrustedHeaderClient.OnBeforeSendHeaders),
  // but the connection set up will be done before that. This is acceptable from
  // Web Request API because the extension has already allowed to set up
  // a connection to the same URL (i.e., the actual request), and distinguishing
  // two connections for the actual request and the preflight request before
  // sending request headers is very difficult.
  const uint64_t url_request_id =
      request_id_generator_->Generate(MSG_ROUTING_NONE, 0);

  auto result = requests_.insert(std::make_pair(
      url_request_id, std::make_unique<InProgressRequest>(
                          this, url_request_id, frame_routing_id_, request)));

  result.first->second->OnLoaderCreated(std::move(receiver));
  result.first->second->Restart();
}

UrlRequestProxyingURLLoaderFactory::~UrlRequestProxyingURLLoaderFactory() =
    default;

void UrlRequestProxyingURLLoaderFactory::OnTargetFactoryError() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  target_factory_.reset();
  proxy_receivers_.Clear();

  MaybeRemoveProxy();
}

void UrlRequestProxyingURLLoaderFactory::OnProxyBindingError() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  MaybeRemoveProxy();
}

void UrlRequestProxyingURLLoaderFactory::RemoveRequest(
    int32_t network_service_request_id,
    uint64_t request_id) {
  network_request_id_to_url_request_id_.erase(network_service_request_id);
  requests_.erase(request_id);
  if (network_service_request_id) {
    proxies_->DisassociateProxyWithRequestId(
        this, content::GlobalRequestID(render_process_id_,
                                       network_service_request_id));
  }

  MaybeRemoveProxy();
}

void UrlRequestProxyingURLLoaderFactory::MaybeRemoveProxy() {
  // We can delete this factory only when
  //  - there are no existing requests, and
  //  - it is impossible for a new request to arrive in the future.
  if (!requests_.empty() || !proxy_receivers_.empty()) {
    return;
  }

  // Deletes |this|.
  proxies_->RemoveProxy(this);
}

// static
void UrlRequestProxyingURLLoaderFactory::EnsureAssociatedFactoryBuilt() {
  ShutdownNotifierFactory::GetInstance();
}

}  // namespace extensions
