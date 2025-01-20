// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Download code which handles CRX files (extensions, themes, apps, ...).

#include "chrome/browser/download/download_crx_util.h"
#include "components/download/public/common/download_item.h"
#include "chrome/browser/download/android/extension_install_dialog_bridge.h"
// #include "extensions/browser/extension_system.h"

#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/webstore_installer.h"
#include "chrome/browser/profiles/profile.h"
// #include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
// #include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/user_script.h"

#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"

#include "ui/android/view_android.h"
#include "ui/android/window_android.h"

using download::DownloadItem;
using extensions::WebstoreInstaller;

#include "extensions/buildflags/buildflags.h"

// This file is used on non-desktop Android where extensions are not supported.
// static_assert(!BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace download_crx_util {

namespace {

// Called to get an extension install UI object.
std::unique_ptr<ExtensionInstallPrompt> CreateExtensionInstallPrompt(
    Profile* profile,
    const DownloadItem& download_item) {

  content::WebContents* web_contents =
      content::DownloadItemUtils::GetWebContents(
          const_cast<DownloadItem*>(&download_item));
  if (!web_contents) {
    for (TabModel* model : TabModelList::models()) {
      if (model->IsActiveModel()) {
        web_contents = model->GetActiveWebContents();
        break;
      }
    }
  }
  return std::make_unique<ExtensionInstallPrompt>(web_contents);
}

}

bool IsExtensionDownload(const download::DownloadItem& download_item) {
  // Extensions are not supported on Android. We want to treat them as
  // normal file downloads.
  // return false;
  if (download_item.GetTargetDisposition() ==
      DownloadItem::TARGET_DISPOSITION_PROMPT)
    return false;

  if (download_item.GetMimeType() == extensions::Extension::kMimeType ||
      extensions::UserScript::IsURLUserScript(download_item.GetURL(),
                                              download_item.GetMimeType())) {
    return true;
  } else {
    return false;
  }
}

bool IsTrustedExtensionDownload(Profile* profile,
                                const download::DownloadItem& item) {
  // Extensions are not supported on Android, return the safe default. Yeah, right.
  return IsExtensionDownload(item);
}

scoped_refptr<extensions::CrxInstaller> CreateCrxInstaller(
    Profile* profile,
    const download::DownloadItem& download_item) {
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();

  CHECK(service);

  auto* approval = WebstoreInstaller::GetAssociatedApproval(download_item);
  // approval->skip_install_dialog = true;
  // approval->skip_post_install_ui = true;

  scoped_refptr<extensions::CrxInstaller> installer(
      extensions::CrxInstaller::Create(
          profile,
          CreateExtensionInstallPrompt(profile, download_item),
          approval));

  installer->set_error_on_unsupported_requirements(true);
  installer->set_delete_source(true);
  // installer->set_install_cause(extension_misc::INSTALL_CAUSE_USER_DOWNLOAD);
  installer->set_original_mime_type(download_item.GetOriginalMimeType());
  installer->set_apps_require_extension_mime_type(true);

  return installer;
}

bool OffStoreInstallAllowedByPrefs(Profile* profile, const DownloadItem& item) {
  return true;
}

}  // namespace download_crx_util
