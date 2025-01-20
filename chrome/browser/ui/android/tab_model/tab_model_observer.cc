// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/tab_model/tab_model_observer.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/android/tab_android.h"
// #include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"

#include "chrome/browser/extensions/api/tabs/tabs_event_router.h"
#include "chrome/browser/extensions/api/tabs/tabs_windows_api.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/profiles/profile.h"
// #include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/sessions/content/session_tab_helper.h"

// chrome/browser/extensions/api/tabs/tabs_event_router.cc
constexpr char kFromIndexKey[] = "fromIndex";
constexpr char kToIndexKey[] = "toIndex";
constexpr char kWindowClosing[] = "isWindowClosing";

TabModelObserver::TabModelObserver() = default;

TabModelObserver::~TabModelObserver() = default;

void TabModelObserver::DidSelectTab(TabAndroid* tab,
                                    TabModel::TabSelectionType type) {
LOG(INFO) << "TabModelObserver::DidSelectTab";
  auto* profile = tab->profile();
  extensions::TabsWindowsAPI* tabs_window_api = extensions::TabsWindowsAPI::Get(profile);
  if (tabs_window_api) {
    base::Value::List args;
    int tab_id = tab->GetTabId().id();
    args.Append(tab_id);
 
    base::Value::Dict object_args;
    object_args.Set(extensions::tabs_constants::kWindowIdKey,
                    tab->GetWindowId().id());
    args.Append(object_args.Clone());
 
    // The onActivated event replaced onActiveChanged and onSelectionChanged. The
    // deprecated events take two arguments: tabId, {windowId}.
    tabs_window_api->tabs_event_router()->DispatchEventAndroid(profile, extensions::events::TABS_ON_SELECTION_CHANGED,
                  extensions::api::tabs::OnSelectionChanged::kEventName, args.Clone(),
                  extensions::EventRouter::UserGestureState::kUnknown);
    tabs_window_api->tabs_event_router()->DispatchEventAndroid(profile, extensions::events::TABS_ON_ACTIVE_CHANGED,
                  extensions::api::tabs::OnActiveChanged::kEventName, std::move(args),
                  extensions::EventRouter::UserGestureState::kUnknown);
 
    // The onActivated event takes one argument: {windowId, tabId}.
    base::Value::List on_activated_args;
    object_args.Set("tabId" /* kTabIdKey */, tab_id);
    on_activated_args.Append(std::move(object_args));
    tabs_window_api->tabs_event_router()->DispatchEventAndroid(
        profile, extensions::events::TABS_ON_ACTIVATED, extensions::api::tabs::OnActivated::kEventName,
        std::move(on_activated_args), extensions::EventRouter::UserGestureState::kUnknown);

  }
}

void TabModelObserver::WillCloseTab(TabAndroid* tab) {}

void TabModelObserver::OnFinishingTabClosure(TabAndroid* tab) {}

void TabModelObserver::OnFinishingMultipleTabClosure(
    const std::vector<raw_ptr<TabAndroid, VectorExperimental>>& tabs,
    bool canRestore) {}

void TabModelObserver::WillAddTab(TabAndroid* tab,
                                  TabModel::TabLaunchType type) {
  // do nothing, see DidAddTab
}

void TabModelObserver::DidAddTab(TabAndroid* tab,
                                 TabModel::TabLaunchType type) {
LOG(INFO) << "TabModelObserver::DidAddTab";
  auto* profile = tab->profile();
  extensions::TabsWindowsAPI* tabs_window_api = extensions::TabsWindowsAPI::Get(profile);
  if (tabs_window_api) {
    TabStripModelChange::Insert insert;
    insert.contents.push_back({ nullptr, tab->web_contents(), tab->GetTabId().id() });
    TabStripModelChange change(std::move(insert));
    TabStripSelectionChange selection;
    tabs_window_api->tabs_event_router()->OnTabStripModelChanged(
      nullptr, /* TabStripModel */
      change,
      selection
    );
  }
}

void TabModelObserver::DidMoveTab(TabAndroid* tab,
                                  int new_index,
                                  int old_index) {
  // a la TabsEventRouter::DispatchTabMoved
  auto* profile = tab->profile();
  extensions::TabsWindowsAPI* tabs_window_api = extensions::TabsWindowsAPI::Get(profile);
  if (tabs_window_api) {
    base::Value::List args;
    args.Append(tab->GetTabId().id());
 
    base::Value::Dict object_args;
    object_args.Set(extensions::tabs_constants::kWindowIdKey,
                    tab->GetWindowId().id());
    object_args.Set(kFromIndexKey, old_index);
    object_args.Set(kToIndexKey, new_index);
    args.Append(std::move(object_args));
 
    tabs_window_api->tabs_event_router()->DispatchEventAndroid(profile, extensions::events::TABS_ON_MOVED, extensions::api::tabs::OnMoved::kEventName,
                  std::move(args), extensions::EventRouter::UserGestureState::kUnknown);
  }
}

void TabModelObserver::TabPendingClosure(TabAndroid* tab) {}

void TabModelObserver::TabClosureUndone(TabAndroid* tab) {}

void TabModelObserver::OnTabCloseUndone(
    const std::vector<raw_ptr<TabAndroid, VectorExperimental>>& tabs) {}

void TabModelObserver::TabClosureCommitted(TabAndroid* tab) {}

void TabModelObserver::AllTabsPendingClosure(
    const std::vector<raw_ptr<TabAndroid, VectorExperimental>>& tabs) {}

void TabModelObserver::AllTabsClosureCommitted() {}

void TabModelObserver::TabRemoved(TabAndroid* tab) {
  auto* profile = tab->profile();
  extensions::TabsWindowsAPI* tabs_window_api = extensions::TabsWindowsAPI::Get(profile);
  if (tabs_window_api) {
    base::Value::List args;
    args.Append(tab->GetTabId().id());
 
    base::Value::Dict object_args;
    object_args.Set(extensions::tabs_constants::kWindowIdKey,
                    tab->GetWindowId().id());
    object_args.Set(kWindowClosing, false); // TODO
    args.Append(std::move(object_args));
 
    tabs_window_api->tabs_event_router()->DispatchEventAndroid(profile, extensions::events::TABS_ON_REMOVED,
                extensions::api::tabs::OnRemoved::kEventName, std::move(args),
                extensions::EventRouter::UserGestureState::kUnknown);
  }
}
