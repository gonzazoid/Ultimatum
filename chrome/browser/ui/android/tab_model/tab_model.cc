// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model.h"

#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "build/android_buildflags.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/synced_window_delegate_android.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router.h"
#include "chrome/browser/sync/sessions/sync_sessions_web_contents_router_factory.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
// #include "chrome/browser/resource_coordinator/tab_lifecycle_unit_external.h"
#include "components/omnibox/browser/location_bar_model_impl.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

// #if BUILDFLAG(IS_DESKTOP_ANDROID)
// #include "chrome/browser/ui/browser_window/public/browser_window_interface.h"  // nogncheck
// #endif

// #include "chrome/common/extensions/extension_constants.h"
 #include "chrome/browser/extensions/extension_tab_util.h"
// #include "content/public/browser/navigation_entry.h"
// #include "content/public/browser/favicon_status.h"
// #include "chrome/browser/ui/recently_audible_helper.h"
// #include "content/public/browser/navigation_controller.h"

using chrome::android::ActivityType;

namespace {
// Must match Java Tab.INVALID_TAB_ID.
// doesn't seems ok to have it here but googlers have it all over the project so we can afford not to care about it
// unless they all of the sudden decide to become decent coders (which is very much unlikely, if history is any indication)
// static constexpr int kInvalidTabId = -1;
// from chrome/browser/extensions/browser_extension_window_controller.cc
// and it's very very VERY wrong!!!
constexpr char kAlwaysOnTopKey[] = "alwaysOnTop";
constexpr char kFocusedKey[] = "focused";
constexpr char kHeightKey[] = "height";
constexpr char kIncognitoKey[] = "incognito";
constexpr char kLeftKey[] = "left";
constexpr char kShowStateKey[] = "state";
constexpr char kTopKey[] = "top";
constexpr char kWidthKey[] = "width";
constexpr char kWindowTypeKey[] = "type";
constexpr char kShowStateValueNormal[] = "normal";
// this one from extensions/common/constants.h it's still wrong to have it here
inline constexpr char kId[] = "id";
// end of wrongness

sync_sessions::OpenTabsUIDelegate* GetOpenTabsUIDelegate(Profile* profile) {
  sync_sessions::SessionSyncService* service =
      SessionSyncServiceFactory::GetForProfile(profile);

  return service->GetOpenTabsUIDelegate();
}

// Returns the initial |SessionID| for |TabModel|. Currently behind a runtime
// flag until support stabilizes on other platforms.
SessionID GetInitialSessionId() {
  if (!TabModel::EnableBrowserWindowInterfaceMobile()) {
    return SessionID::NewUnique();
  }
  return SessionID::InvalidValue();
}
}  // namespace

TabModel::TabModel(Profile* profile,
                   ActivityType activity_type,
                   std::optional<chrome::android::CustomTabProfileType>
                       custom_tab_profile_type,
                   TabModelType tab_model_type)
    : profile_(profile),
      activity_type_(activity_type),
      custom_tab_profile_type_(custom_tab_profile_type),
      tab_model_type_(tab_model_type),
      live_tab_context_(new AndroidLiveTabContext(this)),
      synced_window_delegate_(new browser_sync::SyncedWindowDelegateAndroid(
          this,
          activity_type == ActivityType::kTabbed)),
      session_id_(GetInitialSessionId()) {}

TabModel::~TabModel() = default;

// stick to BrowserExtensionWindowController::CreateWindowValueForExtension
// from chrome/browser/extensions/browser_extension_window_controller.cc
base::DictValue
TabModel::CreateWindowValueForExtension(
    const extensions::Extension* extension,
    extensions::WindowController::PopulateTabBehavior populate_tab_behavior,
    extensions::mojom::ContextType context) {
  base::DictValue dict;

  dict.Set(kId, GetSessionId().id());
  dict.Set(kWindowTypeKey, extensions::api::tabs::ToString(extensions::api::tabs::WindowType::kNormal));
  dict.Set(kFocusedKey, IsActiveModel());
  const Profile* profile = GetProfile();
  dict.Set(kIncognitoKey, profile->IsOffTheRecord());
  dict.Set(kAlwaysOnTopKey, false);

  dict.Set(kShowStateKey, kShowStateValueNormal);

  TabAndroid* tab = GetTabAt(GetActiveIndex()); // may be 0?
  if (tab) {
    gfx::Rect bounds = tab->GetBounds();
    dict.Set(kLeftKey, bounds.x());
    dict.Set(kTopKey, bounds.y());
    dict.Set(kWidthKey, bounds.width());
    dict.Set(kHeightKey, bounds.height());
  }

  if (populate_tab_behavior == extensions::WindowController::kPopulateTabs) {
    dict.Set(extensions::ExtensionTabUtil::kTabsKey, CreateTabList(extension, context));
  }

  return dict;
}

base::ListValue TabModel::CreateTabList(
    const extensions::Extension* extension,
    extensions::mojom::ContextType context) {
  base::ListValue tab_list;
  for (int i = 0; i < GetTabCount(); ++i) {
     auto* tab = GetTabAt(i);
     auto* contents = tab->web_contents();
     extensions::ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior =
    extensions::ExtensionTabUtil::GetScrubTabBehavior(extension, context, contents);
    tab_list.Append(extensions::ExtensionTabUtil::CreateTabObject(contents, scrub_tab_behavior, extension, this, i)
                        .ToValue());
  }

  return tab_list;
}

// extensions::api::tabs::Tab TabModel::CreateTabObject(
//     const extensions::Extension* extension,
//     int tab_index) const {
//   extensions::api::tabs::Tab tab_object;
//   auto* tab = GetTabAt(tab_index);
//   tab_object.index = tab_index;
//   tab_object.id = tab->GetTabId().id();
//   tab_object.window_id = tab->GetWindowId().id();
//   auto* contents = tab->web_contents();
//   if (contents) {
//     tab_object.last_accessed =
//       contents->GetLastActiveTime().InMillisecondsFSinceUnixEpoch();

//     gfx::Size contents_size = contents->GetContainerBounds().size();
//     tab_object.width = contents_size.width();
//     tab_object.height = contents_size.height();

//     tab_object.url = contents->GetLastCommittedURL().spec(); // or GetUrl() ??
//     content::NavigationEntry* pending_entry = contents->GetController().GetPendingEntry();
//     if (pending_entry) {
//       tab_object.pending_url = pending_entry->GetVirtualURL().spec();
//     }
//     tab_object.title = base::UTF16ToUTF8(contents->GetTitle());

    // TODO(tjudkins) This should probably use the LastCommittedEntry() for
    // consistency.
//     content::NavigationEntry* visible_entry = contents->GetController().GetVisibleEntry();
//     if (visible_entry && visible_entry->GetFavicon().valid) {
//       tab_object.fav_icon_url = visible_entry->GetFavicon().url.spec();
//     }

//     auto* audible_helper = RecentlyAudibleHelper::FromWebContents(contents); // TODO does it work?
//     bool audible = false;
//     if (audible_helper) {
//       // WebContents in a tab strip have RecentlyAudible helpers. They endow the
//       // tab with a notion of audibility that has a timeout for quiet periods. Use
//       // that if available.
//       audible = audible_helper->WasRecentlyAudible();
//     } else {
//       // Otherwise use the instantaneous notion of audibility.
//       audible = contents->IsCurrentlyAudible();
//     }
//     tab_object.audible = audible;
//     tab_object.muted_info = extensions::ExtensionTabUtil::CreateMutedInfo(contents);

//     tab_object.status = extensions::ExtensionTabUtil::GetLoadingStatus(contents);
//   }

//   tab_object.discarded = tab->NeedsReload();
//   tab_object.auto_discardable = true;
//   tab_object.frozen = tab->IsFrozen();
//   tab_object.active = tab_index == GetActiveIndex();
//   tab_object.selected = tab_index == GetActiveIndex();
//   tab_object.highlighted = tab_index == GetActiveIndex();
//   tab_object.pinned = live_tab_context_->IsTabPinned(tab_index);

//   tab_object.group_id = -1;
//   // std::optional<tab_groups::TabGroupId> group = tab->GetTabGroupId();
//   std::optional<base::Token> group = tab->GetTabGroupId();
//   if (group.has_value()) {
//     tab_object.group_id = extensions::ExtensionTabUtil::GetGroupId(
//       tab_groups::TabGroupId::FromRawToken(*group)
//     );
//   }

//   tab_object.incognito = tab->IsIncognito();

//   int parent_id = tab->GetParentId(); // it's android id
//   tab_object.opener_tab_id = 0;
//   if (parent_id != kInvalidTabId) {
//     bool opener_is_alive = false;
//     // that's very very bad, we do brutforce because of poor TabModel design
//     // we need something like TabModel::Includes(tabId)
//     for (int i=0; i < GetTabCount(); i++) {
//       auto* curr_tab = GetTabAt(i);
//       if (curr_tab->GetAndroidId() == parent_id) { // compare with android id
//         parent_id = curr_tab->GetTabId().id(); // now we can get conventional tab id
//         opener_is_alive = true;
//         break;
//       }
//     }
//     if (opener_is_alive && parent_id > 0) {
//       tab_object.opener_tab_id = parent_id;
//     }
//   }

//   // ScrubTabForExtension(extension, contents, &tab_object, scrub_tab_behavior);
//   return tab_object;
// }

Profile* TabModel::GetProfile() const {
  return profile_;
}

bool TabModel::IsOffTheRecord() const {
  return GetProfile()->IsOffTheRecord();
}

sync_sessions::SyncedWindowDelegate* TabModel::GetSyncedWindowDelegate() const {
  return synced_window_delegate_.get();
}

SessionID TabModel::GetSessionId() const {
  return session_id_;
}

sessions::LiveTabContext* TabModel::GetLiveTabContext() const {
  return live_tab_context_.get();
}

content::WebContents* TabModel::GetActiveWebContents() const {
  int active_index = GetActiveIndex();
  if (active_index == kInvalidIndex) {
    return nullptr;
  }
  return GetWebContentsAt(active_index);
}

void TabModel::BroadcastSessionRestoreComplete() {
  sync_sessions::SyncSessionsWebContentsRouter* router =
      sync_sessions::SyncSessionsWebContentsRouterFactory::GetForProfile(
          GetProfile());
  if (router) {
    router->NotifySessionRestoreComplete();
  }

  RecordActualSyncedTabsHistogram();
}

// This logic is loosely based off of
// TabContentsSyncedTabDelegate::ShouldSync().
void TabModel::RecordActualSyncedTabsHistogram() {
  sync_sessions::OpenTabsUIDelegate* open_tabs_delegate =
      GetOpenTabsUIDelegate(GetProfile());
  // This null check will early exit if the user is in incognito mode or
  // if the user has opted out of syncing tabs.
  if (!open_tabs_delegate) {
    return;
  }

  // This null check will early exit if the user has a null sync session,
  // which includes but is not limited to: no synced tabs or an opt out of
  // syncing tabs even if sync is enabled.
  const sync_sessions::SyncedSession* local_session = nullptr;
  if (!open_tabs_delegate->GetLocalSession(&local_session)) {
    return;
  }

  // This check will early exit if there are no tabs in the local model.
  if (GetTabCount() == 0) {
    return;
  }

  int synced_tabs_count = 0;
  for (const auto& [window_id, window] : local_session->windows) {
    synced_tabs_count += window->wrapped_window.tabs.size();
  }

  int eligible_tabs_count = 0;
  for (int i = 0; i < GetTabCount(); i++) {
    if (SessionSyncServiceFactory::ShouldSyncURLForTestingAndMetrics(
            GetTabAt(i)->GetURL())) {
      eligible_tabs_count++;
    }
  }

  // Prevent dividing by 0 in case all tabs are filtered out.
  if (eligible_tabs_count == 0) {
    return;
  }

  int percent_synced = synced_tabs_count * 100 / eligible_tabs_count;
  base::UmaHistogramPercentage("Android.Sync.ActualSyncedTabCountPercentage",
                               percent_synced);
}

void TabModel::SetSessionId(SessionID session_id) {
  if (!TabModel::EnableBrowserWindowInterfaceMobile()) {
    LOG(ERROR) << "Setting session ID is not supported yet.";
    return;
  }
  session_id_ = session_id;
}

// static
// From //chrome/browser/tab_list/tab_list_interface.h
bool TabListInterface::CanEditTabList(Profile& profile) {
  for (TabModel* model : TabModelList::models()) {
    if (model->GetProfile() != &profile ||
        model->GetTabModelType() != TabModel::TabModelType::kStandard) {
      continue;
    }

    if (!model->IsThisTabListEditable()) {
      return false;
    }
  }

  return true;
}

// static
bool TabModel::EnableBrowserWindowInterfaceMobile() {
#if BUILDFLAG(IS_DESKTOP_ANDROID)
  return true;
#else   // !BUILDFLAG(IS_DESKTOP_ANDROID)
  return base::FeatureList::IsEnabled(
      chrome::android::kBrowserWindowInterfaceMobile);
#endif  // BUILDFLAG(IS_DESKTOP_ANDROID)
}
