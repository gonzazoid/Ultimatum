// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/context_menu_helper.h"

#include <stdint.h>

#include <map>

#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_util.h"
#include "components/embedder_support/android/contextmenu/context_menu_builder.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/android/view_android.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"


#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"

#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "chrome/browser/extensions/context_menu_matcher.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu.h"
#include "chrome/browser/browser_process.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/text_elider.h"

#include "ui/menus/android/menu_model_bridge.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/android/chrome_jni_headers/ContextMenuHelper_jni.h"

using base::android::JavaParamRef;
using base::android::JavaRef;

using extensions::ContextMenuMatcher;
using extensions::Extension;
using extensions::MenuItem;
using extensions::MenuManager;
using blink::mojom::ContextMenuDataMediaType;

ContextMenuHelper::ContextMenuHelper(content::WebContents* web_contents)
    : content::WebContentsUserData<ContextMenuHelper>(*web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_obj_.Reset(
      env, Java_ContextMenuHelper_create(env, reinterpret_cast<int64_t>(this),
                                         web_contents->GetJavaWebContents())
               .obj());
  DCHECK(!java_obj_.is_null());
}

ContextMenuHelper::~ContextMenuHelper() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextMenuHelper_destroy(env, java_obj_);
}

// borrowed from RenderViewContextMenu::PrintableSelectionText()
std::u16string PrintableSelectionText(const content::ContextMenuParams& params) {
  return gfx::TruncateString(params.selection_text, RenderViewContextMenu::kMaxSelectionTextLength,
                             gfx::WORD_BREAK);
}


bool ExtensionPatternMatch(const extensions::URLPatternSet& patterns,
                           const GURL& url) {
  // No patterns means no restriction, so that implicitly matches.
  if (patterns.is_empty()) {
    return true;
  }
  return patterns.MatchesURL(url);
}

bool ExtensionContextAndPatternMatch(
    const content::ContextMenuParams& params,
    const MenuItem::ContextList& contexts,
    const extensions::URLPatternSet& target_url_patterns) {
  const bool has_link = !params.link_url.is_empty();
  const bool has_selection = !params.selection_text.empty();
  const bool in_subframe = params.is_subframe;

  if (contexts.Contains(MenuItem::ALL) ||
      (has_selection && contexts.Contains(MenuItem::SELECTION)) ||
      (params.is_editable && contexts.Contains(MenuItem::EDITABLE)) ||
      (in_subframe && contexts.Contains(MenuItem::FRAME))) {
    return true;
  }

  if (has_link && contexts.Contains(MenuItem::LINK) &&
      ExtensionPatternMatch(target_url_patterns, params.link_url)) {
    return true;
  }

  switch (params.media_type) {
    case ContextMenuDataMediaType::kImage:
      if (contexts.Contains(MenuItem::IMAGE) &&
          ExtensionPatternMatch(target_url_patterns, params.src_url)) {
        return true;
      }
      break;

    case ContextMenuDataMediaType::kVideo:
      if (contexts.Contains(MenuItem::VIDEO) &&
          ExtensionPatternMatch(target_url_patterns, params.src_url)) {
        return true;
      }
      break;

    case ContextMenuDataMediaType::kAudio:
      if (contexts.Contains(MenuItem::AUDIO) &&
          ExtensionPatternMatch(target_url_patterns, params.src_url)) {
        return true;
      }
      break;

    default:
      break;
  }

  // PAGE is the least specific context, so we only examine that if none of the
  // other contexts apply (except for FRAME, which is included in PAGE for
  // backwards compatibility).
  if (!has_link && !has_selection && !params.is_editable &&
      params.media_type == ContextMenuDataMediaType::kNone &&
      contexts.Contains(MenuItem::PAGE)) {
    return true;
  }

  return false;
}

bool MenuItemMatchesParams(
    const content::ContextMenuParams& params,
    const extensions::MenuItem* item) {
  bool match = ExtensionContextAndPatternMatch(params, item->contexts(),
                                               item->target_url_patterns());
  if (!match) {
    return false;
  }

  return ExtensionPatternMatch(item->document_url_patterns(), params.frame_url);
}

void ContextMenuHelper::ShowContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  LOG(INFO) << "ContextMenuHelper::ShowContextMenu";
  // let's check if we can get extensions contextMenus params
  content::BrowserContext* browser_context = render_frame_host.GetBrowserContext();

  std::unique_ptr<ui::SimpleMenuModel> context_menu_model = std::make_unique<ui::SimpleMenuModel>(nullptr);

  extensions::ContextMenuMatcher extension_items(
    browser_context,
    nullptr,
    context_menu_model.get(),
    base::BindRepeating(MenuItemMatchesParams, params)
  );

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser_context);

  MenuManager* menu_manager = MenuManager::Get(browser_context);
  if (!menu_manager) {
    return;
  }

  std::u16string printable_selection_text = PrintableSelectionText(params);
  // EscapeAmpersands(&printable_selection_text);

  // Get a list of extension id's that have context menu items, and sort by the
  // top level context menu title of the extension.
  std::vector<std::u16string> sorted_menu_titles;
  std::map<std::u16string, std::vector<const Extension*>>
      title_to_extensions_map;
  for (const auto& id : menu_manager->ExtensionIds()) {
    const Extension* extension =
        registry->enabled_extensions().GetByID(id.extension_id);
    // Platform apps have their context menus created directly in
    // AppendPlatformAppItems.
    if (extension && !extension->is_platform_app()) {
      std::u16string menu_title = extension_items.GetTopLevelContextMenuTitle(
          id, printable_selection_text);
      title_to_extensions_map[menu_title].push_back(extension);
      sorted_menu_titles.push_back(menu_title);
    }
  }
  if (sorted_menu_titles.empty()) {
    return;
  }

  const std::string app_locale = g_browser_process->GetApplicationLocale();
  l10n_util::SortStrings16(app_locale, &sorted_menu_titles);
  sorted_menu_titles.erase(
      std::unique(sorted_menu_titles.begin(), sorted_menu_titles.end()),
      sorted_menu_titles.end());

  int index = 0;
  for (const auto& title : sorted_menu_titles) {
    const std::vector<const Extension*>& extensions =
        title_to_extensions_map[title];
    for (const Extension* extension : extensions) {
      MenuItem::ExtensionKey extension_key(extension->id());
      LOG(INFO) << "READY TO ADD!!!" << extension_key.extension_id << "::" << title;
      extension_items.AppendExtensionItems(extension_key,
                                            printable_selection_text, &index,
                                            /*is_action_menu=*/false);
    }
  }

  // now lets convert it to java object
  LOG(INFO) << "BEFORE new ui::MenuModelBridge";
  ui::MenuModelBridge* bridge = new ui::MenuModelBridge();
  LOG(INFO) << "BEFORE bridge->AddExtensionItems";
  bridge->AddExtensionItems(context_menu_model.get());
  LOG(INFO) << "AFTER bridge->AddExtensionItems";

  // ==============================================
  JNIEnv* env = base::android::AttachCurrentThread();
  context_menu_params_ = params;
  gfx::NativeView view = GetWebContents().GetNativeView();

  LOG(INFO) << "BEFORE Java_ContextMenuHelper_setExtensionsMenu";
  Java_ContextMenuHelper_setExtensionsMenu(env, java_obj_, bridge->GetListItems());
  LOG(INFO) << "AFTER Java_ContextMenuHelper_setExtensionsMenu";

  Java_ContextMenuHelper_showContextMenu(
      env, java_obj_,
      context_menu::BuildJavaContextMenuParams(
          context_menu_params_,
          render_frame_host.GetProcess()->GetDeprecatedID(),
          render_frame_host.GetFrameToken().value()),
      render_frame_host.GetJavaRenderFrameHost(), view->GetContainerView(),
      view->content_offset() * view->GetDipScale());
}

void ContextMenuHelper::DismissContextMenu() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextMenuHelper_dismissContextMenu(env, java_obj_);
}

void ContextMenuHelper::OnContextMenuClosed(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  GetWebContents().NotifyContextMenuClosed(context_menu_params_.link_followed,
                                           context_menu_params_.impression);
}

void ContextMenuHelper::SetPopulatorFactory(
    const JavaRef<jobject>& jpopulator_factory) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_ContextMenuHelper_setPopulatorFactory(env, java_obj_,
                                             jpopulator_factory);
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContextMenuHelper);
