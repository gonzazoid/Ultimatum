// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/desktop_android/desktop_android_extensions_browser_client.h"

#include <memory>
#include <utility>

#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/management/chrome_management_api_delegate.h"
#include "chrome/browser/extensions/chrome_extension_system_factory.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/api/preference/network_prediction_transformer.h"
#include "chrome/browser/extensions/api/proxy/proxy_pref_transformer.h"
#include "chrome/browser/extensions/chrome_extensions_browser_api_provider.h"
#include "chrome/browser/extensions/desktop_android/desktop_android_extension_host_delegate.h"
#include "chrome/browser/extensions/desktop_android/desktop_android_runtime_api_delegate.h"
#include "chrome/browser/extensions/error_console/error_console.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/prefetch/pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "components/update_client/update_client.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/pref_mapping.h"
#include "chrome/browser/supervised_user/supervised_user_extensions_delegate_impl.h"
#include "components/version_info/version_info.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "extensions/browser/api/core_extensions_browser_api_provider.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/messaging/messaging_delegate.h"
#include "chrome/browser/extensions/api/chrome_extensions_api_client.h"
#include "extensions/browser/api/declarative_content/content_rules_registry.h"
#include "chrome/browser/extensions/api/declarative_content/chrome_content_rules_registry.h"
#include "chrome/browser/extensions/api/declarative_content/default_content_predicate_evaluators.h"
#include "chrome/browser/extensions/api/management/chrome_management_api_delegate.h"
#include "extensions/browser/api/mime_handler_private/mime_handler_private.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_error.h"
#include "extensions/browser/extension_util.h"
#include "extensions/browser/extension_web_contents_observer.h"
#include "extensions/browser/extensions_browser_interface_binders.h"
#include "extensions/browser/kiosk/kiosk_delegate.h"
#include "extensions/browser/null_app_sorting.h"
#include "extensions/browser/updater/null_extension_cache.h"
#include "extensions/browser/url_request_util.h"
#include "extensions/common/features/feature_channel.h"
#include "extensions/common/api/mime_handler.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"

#include "extensions/browser/api/content_settings/content_settings_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#endif

using content::BrowserContext;
using content::BrowserThread;

namespace extensions {

namespace {

bool RegisterTransformers() {
  PrefMapping* pref_mapping = PrefMapping::GetInstance();
  // pref_mapping->RegisterPrefTransformer(
  //     prefs::kCookieControlsMode,
  //     std::make_unique<CookieControlsModeTransformer>());
  pref_mapping->RegisterPrefTransformer(
      proxy_config::prefs::kProxy, std::make_unique<ProxyPrefTransformer>());
  pref_mapping->RegisterPrefTransformer(
      prefetch::prefs::kNetworkPredictionOptions,
      std::make_unique<NetworkPredictionTransformer>());
  // pref_mapping->RegisterPrefTransformer(
  //     prefs::kProtectedContentDefault,
  //     std::make_unique<ProtectedContentEnabledTransformer>());
  // pref_mapping->RegisterPrefTransformer(
  //     prefs::kPrivacySandboxM1TopicsEnabled,
  //     std::make_unique<PrivacySandboxTransformer>());
  // pref_mapping->RegisterPrefTransformer(
  //     prefs::kPrivacySandboxM1FledgeEnabled,
  //     std::make_unique<PrivacySandboxTransformer>());
  // pref_mapping->RegisterPrefTransformer(
  //     prefs::kPrivacySandboxM1AdMeasurementEnabled,
  //     std::make_unique<PrivacySandboxTransformer>());
  // pref_mapping->RegisterPrefTransformer(
  //     prefs::kPrivacySandboxRelatedWebsiteSetsEnabled,
  //     std::make_unique<PrivacySandboxTransformer>());

  return true;
}

DesktopAndroidExtensionsBrowserClient* g_extension_browser_client = nullptr;

class DesktopAndroidKioskDelegate : public KioskDelegate {
 public:
  DesktopAndroidKioskDelegate() = default;
  ~DesktopAndroidKioskDelegate() override = default;

  bool IsAutoLaunchedKioskApp(const ExtensionId& id) const override {
    // Desktop-android does not support kiosk apps.
    return false;
  }
};

class DesktopAndroidExtensionsAPIClient : public ExtensionsAPIClient {
 public:
  DesktopAndroidExtensionsAPIClient() = default;
  ~DesktopAndroidExtensionsAPIClient() override = default;

  // ExtensionsAPIClient:
  MessagingDelegate* GetMessagingDelegate() override {
    // The default implementation does nothing, which is fine for now, since
    // this is mostly needed for:
    //   a) tab-specifics,
    //   b) platform apps, and
    //   c) native messaging
    if (!messaging_delegate_) {
      messaging_delegate_ = std::make_unique<MessagingDelegate>();
    }
    return messaging_delegate_.get();
  }

  std::unique_ptr<SupervisedUserExtensionsDelegate>
  CreateSupervisedUserExtensionsDelegate(
    content::BrowserContext* browser_context) const override {
    return std::make_unique<SupervisedUserExtensionsDelegateImpl>(
      browser_context);
  }

  scoped_refptr<ContentRulesRegistry>
  CreateContentRulesRegistry(
      content::BrowserContext* browser_context,
      RulesCacheDelegate* cache_delegate) const override {
    return base::MakeRefCounted<ChromeContentRulesRegistry>(
        browser_context, cache_delegate,
        base::BindOnce(&CreateDefaultContentPredicateEvaluators,
                       base::Unretained(browser_context)));
  }

  ManagementAPIDelegate* CreateManagementAPIDelegate() const override {
    // `ManagementAPI` owns the object.
    return new ChromeManagementAPIDelegate;
  }

 private:
  std::unique_ptr<MessagingDelegate> messaging_delegate_;
};

}  // namespace

DesktopAndroidExtensionsBrowserClient::DesktopAndroidExtensionsBrowserClient()
    : extension_cache_(std::make_unique<NullExtensionCache>()),
      kiosk_delegate_(std::make_unique<DesktopAndroidKioskDelegate>()) // ,
      /* api_client_(std::make_unique<DesktopAndroidExtensionsAPIClient>()) */ {
  AddAPIProvider(std::make_unique<CoreExtensionsBrowserAPIProvider>());
  AddAPIProvider(std::make_unique<ChromeExtensionsBrowserAPIProvider>());

  static bool registered = RegisterTransformers();
  CHECK(registered);

  api_client_ = std::make_unique<ChromeExtensionsAPIClient>();
}

DesktopAndroidExtensionsBrowserClient::
    ~DesktopAndroidExtensionsBrowserClient() = default;

DesktopAndroidExtensionsBrowserClient* DesktopAndroidExtensionsBrowserClient::Get() {
  return g_extension_browser_client;
}

void DesktopAndroidExtensionsBrowserClient::Set(DesktopAndroidExtensionsBrowserClient* client) {
  g_extension_browser_client = client;
}

bool DesktopAndroidExtensionsBrowserClient::IsShuttingDown() {
  return false;
}

bool DesktopAndroidExtensionsBrowserClient::AreExtensionsDisabled(
    const base::CommandLine& command_line,
    BrowserContext* context) {
  return false;
}

bool DesktopAndroidExtensionsBrowserClient::IsValidTabId(content::BrowserContext* browser_context,
    int tab_id,
    bool include_incognito,
    content::WebContents** web_contents) const {
  return ExtensionTabUtil::GetTabById(tab_id, browser_context, include_incognito, nullptr, web_contents, nullptr);
}

ScriptExecutor* DesktopAndroidExtensionsBrowserClient::GetScriptExecutorForTab(
    content::WebContents& web_contents) {
  TabHelper* tab_helper = TabHelper::FromWebContents(&web_contents);
  return tab_helper ? tab_helper->script_executor() : nullptr;
}

bool DesktopAndroidExtensionsBrowserClient::IsValidContext(void* context) {
  if (!g_browser_process) {
    LOG(ERROR) << "Unexpected null g_browser_process";
    NOTREACHED();
  }
  return g_browser_process->profile_manager() &&
         g_browser_process->profile_manager()->IsValidProfile(context);
}

bool DesktopAndroidExtensionsBrowserClient::IsSameContext(
    BrowserContext* first,
    BrowserContext* second) {
  Profile* first_profile = Profile::FromBrowserContext(first);
  Profile* second_profile = Profile::FromBrowserContext(second);
  return first_profile->IsSameOrParent(second_profile);
}

bool DesktopAndroidExtensionsBrowserClient::HasOffTheRecordContext(
    BrowserContext* context) {
  return static_cast<Profile*>(context)->HasPrimaryOTRProfile();
}

BrowserContext* DesktopAndroidExtensionsBrowserClient::GetOffTheRecordContext(
    BrowserContext* context) {
  return static_cast<Profile*>(context)->GetPrimaryOTRProfile(
      /*create_if_needed=*/true);
}

BrowserContext* DesktopAndroidExtensionsBrowserClient::GetOriginalContext(
    BrowserContext* context) {
  return static_cast<Profile*>(context)->GetOriginalProfile();
}

content::BrowserContext*
DesktopAndroidExtensionsBrowserClient::GetContextRedirectedToOriginal(
    content::BrowserContext* context) {
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kRedirectedToOriginal)
      .WithGuest(ProfileSelection::kRedirectedToOriginal)
      .Build()
      .ApplyProfileSelection(Profile::FromBrowserContext(context));
}

content::BrowserContext*
DesktopAndroidExtensionsBrowserClient::GetContextOwnInstance(
    content::BrowserContext* context) {
  return context;
}

content::BrowserContext*
DesktopAndroidExtensionsBrowserClient::GetContextForOriginalOnly(
    content::BrowserContext* context) {
  return context;
}

bool DesktopAndroidExtensionsBrowserClient::AreExtensionsDisabledForContext(
    content::BrowserContext* context) {
  return false;
}

bool DesktopAndroidExtensionsBrowserClient::IsGuestSession(
    BrowserContext* context) const {
  return false;
}

bool DesktopAndroidExtensionsBrowserClient::IsExtensionIncognitoEnabled(
    const std::string& extension_id,
    content::BrowserContext* context) const {
  return IsGuestSession(context) ||
         util::IsIncognitoEnabled(extension_id, context);
}

bool DesktopAndroidExtensionsBrowserClient::CanExtensionCrossIncognito(
    const Extension* extension,
    content::BrowserContext* context) const {
  return IsGuestSession(context) || util::CanCrossIncognito(extension, context);
}

base::FilePath DesktopAndroidExtensionsBrowserClient::GetBundleResourcePath(
    const network::ResourceRequest& request,
    const base::FilePath& extension_resources_path,
    int* resource_id) const {
  *resource_id = 0;
  return base::FilePath();
}

void DesktopAndroidExtensionsBrowserClient::LoadResourceFromResourceBundle(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    const base::FilePath& resource_relative_path,
    int resource_id,
    scoped_refptr<net::HttpResponseHeaders> headers,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client) {
  NOTREACHED() << "Load resources from bundles not supported.";
}

bool DesktopAndroidExtensionsBrowserClient::AllowCrossRendererResourceLoad(
    const network::ResourceRequest& request,
    network::mojom::RequestDestination destination,
    ui::PageTransition page_transition,
    int child_id,
    bool is_incognito,
    const Extension* extension,
    const ExtensionSet& extensions,
    const ProcessMap& process_map,
    const GURL& upstream_url) {
  bool allowed = false;
  if (url_request_util::AllowCrossRendererResourceLoad(
          request, destination, page_transition, child_id, is_incognito,
          extension, extensions, process_map, upstream_url, &allowed)) {
    return allowed;
  }

  // Couldn't determine if resource is allowed. Block the load.
  return false;
}

PrefService* DesktopAndroidExtensionsBrowserClient::GetPrefServiceForContext(
    BrowserContext* context) {
  return static_cast<Profile*>(context)->GetPrefs();
}

void DesktopAndroidExtensionsBrowserClient::GetEarlyExtensionPrefsObservers(
    content::BrowserContext* context,
    std::vector<EarlyExtensionPrefsObserver*>* observers) const {
  observers->push_back(ContentSettingsService::Get(context));
}

ProcessManagerDelegate*
DesktopAndroidExtensionsBrowserClient::GetProcessManagerDelegate() const {
  return nullptr;
}

mojo::PendingRemote<network::mojom::URLLoaderFactory>
DesktopAndroidExtensionsBrowserClient::GetControlledFrameEmbedderURLLoader(
    const url::Origin& app_origin,
    content::FrameTreeNodeId frame_tree_node_id,
    content::BrowserContext* browser_context) {
  return mojo::PendingRemote<network::mojom::URLLoaderFactory>();
}

std::unique_ptr<ExtensionHostDelegate>
DesktopAndroidExtensionsBrowserClient::CreateExtensionHostDelegate() {
  return std::make_unique<DesktopAndroidExtensionHostDelegate>();
}

bool DesktopAndroidExtensionsBrowserClient::DidVersionUpdate(
    BrowserContext* context) {
  return false;
}

void DesktopAndroidExtensionsBrowserClient::PermitExternalProtocolHandler() {}

bool DesktopAndroidExtensionsBrowserClient::IsInDemoMode() {
  return false;
}

bool DesktopAndroidExtensionsBrowserClient::IsScreensaverInDemoMode(
    const std::string& app_id) {
  return false;
}

bool DesktopAndroidExtensionsBrowserClient::IsRunningInForcedAppMode() {
  return false;
}

bool DesktopAndroidExtensionsBrowserClient::IsAppModeForcedForApp(
    const ExtensionId& extension_id) {
  return false;
}

bool DesktopAndroidExtensionsBrowserClient::IsLoggedInAsPublicAccount() {
  return false;
}

ExtensionSystemProvider*
DesktopAndroidExtensionsBrowserClient::GetExtensionSystemFactory() {
  return ChromeExtensionSystemFactory::GetInstance();
}

void BindMimeHandlerService(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<mime_handler::MimeHandlerService> receiver) {
  auto* guest_view = MimeHandlerViewGuest::FromRenderFrameHost(frame_host);
  if (!guest_view) {
    return;
  }
  MimeHandlerServiceImpl::Create(guest_view->GetStreamWeakPtr(),
                                 std::move(receiver));
}

void BindBeforeUnloadControl(
    content::RenderFrameHost* frame_host,
    mojo::PendingReceiver<mime_handler::BeforeUnloadControl> receiver) {
  auto* guest_view = MimeHandlerViewGuest::FromRenderFrameHost(frame_host);
  if (!guest_view) {
    return;
  }
  guest_view->FuseBeforeUnloadControl(std::move(receiver));
}

void DesktopAndroidExtensionsBrowserClient::
    RegisterBrowserInterfaceBindersForFrame(
        mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map,
        content::RenderFrameHost* render_frame_host,
        const Extension* extension) const {
  PopulateExtensionFrameBinders(binder_map, render_frame_host, extension);

  binder_map->Add<mime_handler::MimeHandlerService>(
      base::BindRepeating(&BindMimeHandlerService));
  binder_map->Add<mime_handler::BeforeUnloadControl>(
      base::BindRepeating(&BindBeforeUnloadControl));
}

std::unique_ptr<RuntimeAPIDelegate>
DesktopAndroidExtensionsBrowserClient::CreateRuntimeAPIDelegate(
    content::BrowserContext* context) const {
  return std::make_unique<DesktopAndroidRuntimeApiDelegate>(context);
}

const ComponentExtensionResourceManager*
DesktopAndroidExtensionsBrowserClient::GetComponentExtensionResourceManager() {
  return nullptr;
}

void DesktopAndroidExtensionsBrowserClient::BroadcastEventToRenderers(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    base::Value::List args,
    bool dispatch_to_off_the_record_profiles) {}

ExtensionCache* DesktopAndroidExtensionsBrowserClient::GetExtensionCache() {
  return extension_cache_.get();
}

bool DesktopAndroidExtensionsBrowserClient::IsBackgroundUpdateAllowed() {
  return true;
}

bool DesktopAndroidExtensionsBrowserClient::IsMinBrowserVersionSupported(
    const std::string& min_version) {
  return true;
}

void DesktopAndroidExtensionsBrowserClient::ReportError(
    content::BrowserContext* context,
    std::unique_ptr<ExtensionError> error) {
  LOG(ERROR) << error->GetDebugString();
  ErrorConsole::Get(context)->ReportError(std::move(error));
}

void DesktopAndroidExtensionsBrowserClient::CreateExtensionWebContentsObserver(
    content::WebContents* web_contents) {
  ChromeExtensionWebContentsObserver::CreateForWebContents(web_contents);
}

ExtensionWebContentsObserver*
DesktopAndroidExtensionsBrowserClient::GetExtensionWebContentsObserver(
    content::WebContents* web_contents) {
  return ChromeExtensionWebContentsObserver::FromWebContents(web_contents);
}

scoped_refptr<update_client::UpdateClient>
DesktopAndroidExtensionsBrowserClient::CreateUpdateClient(
    content::BrowserContext* context) {
  return util::CreateUpdateClient(context);
}

KioskDelegate* DesktopAndroidExtensionsBrowserClient::GetKioskDelegate() {
  return kiosk_delegate_.get();
}

std::string DesktopAndroidExtensionsBrowserClient::GetApplicationLocale() {
  return "en-US";
}

void DesktopAndroidExtensionsBrowserClient::GetTabAndWindowIdForWebContents(
    content::WebContents* web_contents,
    int* tab_id,
    int* window_id) {
  for (TabModel* model : TabModelList::models()) {
    for (int i = 0; i < model->GetTabCount(); i++) {
      // yeah, I'm ashamed of myself too
      content::WebContents* contents = model->GetWebContentsAt(i);
      if (contents == web_contents) {
        *tab_id = model->GetTabAt(i)->GetTabId().id();
        *window_id = model->GetTabAt(i)->GetWindowId().id();
        return;
      }
    }
  }

  *tab_id = -1;
  *window_id = -1;
}

}  // namespace extensions
