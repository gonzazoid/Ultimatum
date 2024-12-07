// Copyright 2012 The Chromium Authors
// Copyright 2024 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_URLREQUEST_URLREQUEST_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_URLREQUEST_URLREQUEST_API_H_

#include <string>

#include "base/containers/unique_ptr_adapters.h"
#include "extensions/browser/api/url_request/extension_url_request_event_router.h"
#include "extensions/browser/api/url_request/url_request_api_constants.h"
#include "extensions/browser/api/url_request/url_request_api_helpers.h"
#include "chrome/common/extensions/api/url_request.h"
#include "content/public/browser/content_browser_client.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

class Profile;

namespace extensions {

// Observes a single DownloadManager and many DownloadItems and dispatches
// onCreated and onErased events.
class UrlRequestAPI : public BrowserContextKeyedAPI,
                      public EventRouter::Observer,
                      public ExtensionRegistryObserver {
 public:

  explicit UrlRequestAPI(content::BrowserContext* context);

  UrlRequestAPI(const UrlRequestAPI&) = delete;
  UrlRequestAPI& operator=(const UrlRequestAPI&) = delete;

  ~UrlRequestAPI() override;

  // An interface which is held by ProxySet defined below.
  class Proxy {
   public:
    virtual ~Proxy() = default;

  };

  // A ProxySet is a set of proxies used by UrlRequestAPI: It holds Proxy
  // instances, and removes all proxies when it is destroyed.
  class ProxySet {
   public:
    ProxySet();

    ProxySet(const ProxySet&) = delete;
    ProxySet& operator=(const ProxySet&) = delete;

    ~ProxySet();

    // Add a Proxy.
    void AddProxy(std::unique_ptr<Proxy> proxy);
    // Remove a Proxy. The removed proxy is deleted upon this call.
    void RemoveProxy(Proxy* proxy);

    // Associates |proxy| with |id|. |proxy| must already be registered within
    // this ProxySet.
    //
    // Each Proxy may be responsible for multiple requests, but any given
    // request identified by |id| must be associated with only a single proxy.
    void AssociateProxyWithRequestId(Proxy* proxy,
                                     const content::GlobalRequestID& id);

    // Disassociates |proxy| with |id|. |proxy| must already be registered
    // within this ProxySet.
    void DisassociateProxyWithRequestId(Proxy* proxy,
                                        const content::GlobalRequestID& id);

    Proxy* GetProxyFromRequestId(const content::GlobalRequestID& id);

   private:
    // Although these members are initialized on the UI thread, we expect at
    // least one memory barrier before actually calling Generate in the IO
    // thread, so we don't protect them with a lock.
    std::set<std::unique_ptr<Proxy>, base::UniquePtrComparator> proxies_;

    // Bi-directional mapping between request ID and Proxy for faster lookup.
    std::map<content::GlobalRequestID, Proxy*> request_id_to_proxy_map_;
    std::map<Proxy*, std::set<content::GlobalRequestID>>
        proxy_to_request_id_map_;
  };

  class RequestIDGenerator {
   public:
    RequestIDGenerator();

    RequestIDGenerator(const RequestIDGenerator&) = delete;
    RequestIDGenerator& operator=(const RequestIDGenerator&) = delete;

    ~RequestIDGenerator();

    // Generates a UrlRequest ID. If the same (routing_id,
    // network_service_request_id) pair is passed to this as was previously
    // passed to SaveID(), the |request_id| passed to SaveID() will be returned.
    int64_t Generate(int32_t routing_id, int32_t network_service_request_id);

    // This saves a UrlRequest ID mapped to the (routing_id,
    // network_service_request_id) pair. Clients must call Generate() with the
    // same ID pair to retrieve the |request_id|, or else there may be a memory
    // leak.
    void SaveID(int32_t routing_id,
                int32_t network_service_request_id,
                uint64_t request_id);

   private:
    int64_t id_ = 0;
    std::map<std::pair<int32_t, int32_t>, uint64_t> saved_id_map_;
  };

  // Indicates whether or not the UrlRequestAPI may have one or more proxies
  // installed to support the API.
  bool MayHaveProxies() const;

  // Indicates whether the UrlRequestAPI is available to a RenderFrameHost
  // that embeds a WebView instance.
  bool IsAvailableToWebViewEmbedderFrame(
      content::RenderFrameHost* render_frame_host) const;

  // Returns |true| if the URLLoaderFactory will be proxied; |false| otherwise.
  bool MaybeProxyURLLoaderFactory(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* frame,
      int render_process_id,
      content::ContentBrowserClient::URLLoaderFactoryType type,
      std::optional<int64_t> navigation_id,
      ukm::SourceIdObj ukm_source_id,
      network::URLLoaderFactoryBuilder& factory_builder,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
          header_client,
      scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner,
      const url::Origin& request_initiator = url::Origin());

  // BrowserContextKeyedAPI implementation.
  static BrowserContextKeyedAPIFactory<UrlRequestAPI>* GetFactoryInstance();

  // Checks if |MayHaveProxies()| has changed from false to true, and resets
  // URLLoaderFactories if so.
  void UpdateMayHaveProxies();

  // ExtensionRegistryObserver implementation.
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const Extension* extension) override;
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // extensions::EventRouter::Observer.
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const extensions::EventListenerInfo& details) override;

 private:
  friend class BrowserContextKeyedAPIFactory<UrlRequestAPI>;

  // A count of active extensions for this BrowserContext that use web request
  // permissions.
  int url_request_extension_count_ = 0;

  raw_ptr<content::BrowserContext> browser_context_;
  int render_process_id_;
  RequestIDGenerator request_id_generator_;
  std::unique_ptr<ProxySet> proxies_;

  // Stores the last result of |MayHaveProxies()|, so it can be used in
  // |UpdateMayHaveProxies()|.
  bool may_have_proxies_;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() {
    return "UrlRequestAPI";
  }
};

class UrlRequestInternalFunction : public ExtensionFunction {
 public:
  UrlRequestInternalFunction() = default;

 protected:
  ~UrlRequestInternalFunction() override = default;

  const ExtensionId& extension_id_safe() const {
    return extension() ? extension_id() : base::EmptyString();
  }
};

class UrlRequestInternalAddEventListenerFunction
    : public UrlRequestInternalFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("urlRequestInternal.addEventListener",
                             URLREQUESTINTERNAL_ADDEVENTLISTENER)

 protected:
  ~UrlRequestInternalAddEventListenerFunction() override = default;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class UrlRequestInternalEventHandledFunction
    : public UrlRequestInternalFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("urlRequestInternal.eventHandled",
                             URLREQUESTINTERNAL_EVENTHANDLED)

 protected:
  ~UrlRequestInternalEventHandledFunction() override = default;

 private:
  // Unblocks the network request. Use this function when handling incorrect
  // requests from the extension that cannot be detected by the schema
  // validator.
  void OnError(const std::string& event_name,
               const std::string& sub_event_name,
               uint64_t request_id,
               int render_process_id,
               int web_view_instance_id,
               std::unique_ptr<UrlRequestEventRouter::EventResponse> response);

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_URLREQUEST_URLREQUEST_API_H_
