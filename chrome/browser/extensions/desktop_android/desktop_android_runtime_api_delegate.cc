// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/desktop_android/desktop_android_runtime_api_delegate.h"

#include "base/notimplemented.h"
#include "chrome/browser/extensions/updater/extension_updater.h"
#include "chrome/browser/extensions/extension_tab_util.h"

#include "base/task/single_thread_task_runner.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/common/api/runtime.h"

namespace extensions {

DesktopAndroidRuntimeApiDelegate::DesktopAndroidRuntimeApiDelegate(
    content::BrowserContext* context)
    : browser_context_(context) {}

DesktopAndroidRuntimeApiDelegate::~DesktopAndroidRuntimeApiDelegate() = default;

void DesktopAndroidRuntimeApiDelegate::AddUpdateObserver(
    UpdateObserver* observer) {
  registered_for_updates_ = true;
  ExtensionUpdater::Get(browser_context_)->AddObserver(observer);
}

void DesktopAndroidRuntimeApiDelegate::RemoveUpdateObserver(
    UpdateObserver* observer) {
  if (registered_for_updates_) {
    ExtensionUpdater::Get(browser_context_)->RemoveObserver(observer);
  }
}

// void DesktopAndroidRuntimeApiDelegate::ReloadExtension(
//     const ExtensionId& extension_id) {
//   // TODO(crbug.com/373434594): Support reload.
//   NOTIMPLEMENTED();
// }

// from chrome/browser/extensions/api/runtime/chrome_runtime_api_delegate.cc
void DesktopAndroidRuntimeApiDelegate::ReloadExtension(
    const extensions::ExtensionId& extension_id) {
  // const Extension* extension =
  //     extensions::ExtensionRegistry::Get(browser_context_)
  //         ->GetInstalledExtension(extension_id);
  // int fast_reload_time = kFastReloadTime;
  // int fast_reload_count = extensions::RuntimeAPI::kFastReloadCount;

  // If an extension is unpacked, we allow for a faster reload interval
  // and more fast reload attempts before terminating the extension.
  // This is intended to facilitate extension testing for developers.
  // if (extensions::Manifest::IsUnpackedLocation(extension->location())) {
  //   fast_reload_time = kUnpackedFastReloadTime;
  //   fast_reload_count = extensions::RuntimeAPI::kUnpackedFastReloadCount;
  // }

  // std::pair<base::TimeTicks, int>& reload_info =
  //     last_reload_time_[extension_id];
  // base::TimeTicks now =
  //     g_test_clock ? g_test_clock->NowTicks() : base::TimeTicks::Now();
  // if (reload_info.first.is_null() ||
  //     (now - reload_info.first).InMilliseconds() > fast_reload_time) {
  //   reload_info.second = 0;
  // } else {
  //   reload_info.second++;
  // }
  // if (!reload_info.first.is_null()) {
  //   UMA_HISTOGRAM_LONG_TIMES("Extensions.RuntimeReloadTime",
  //                            now - reload_info.first);
  // }
  // UMA_HISTOGRAM_COUNTS_100("Extensions.RuntimeReloadFastCount",
  //                          reload_info.second);
  // reload_info.first = now;

  extensions::ExtensionRegistrar* registrar =
      extensions::ExtensionRegistrar::Get(browser_context_);
  // if (reload_info.second >= fast_reload_count) {
  //   // Unloading an extension clears all warnings, so first terminate the
  //   // extension, and then add the warning. Since this is called from an
  //   // extension function unloading the extension has to be done
  //   // asynchronously. Fortunately PostTask guarentees FIFO order so just
  //   // post both tasks.
  //   base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
  //       FROM_HERE,
  //       base::BindOnce(&extensions::ExtensionRegistrar::TerminateExtension,
  //                      registrar->GetWeakPtr(), extension_id));
  //   extensions::WarningSet warnings;
  //   warnings.insert(
  //       extensions::Warning::CreateReloadTooFrequentWarning(extension_id));
  //   base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
  //       FROM_HERE,
  //       base::BindOnce(&extensions::WarningService::NotifyWarningsOnUI,
  //                      // TODO(crbug.com/40061562): Remove
  //                      // `UnsafeDanglingUntriaged`
  //                      base::UnsafeDanglingUntriaged(browser_context_),
  //                      warnings));
  // } else {
  //   // We can't call ReloadExtension directly, since when this method finishes
  //   // it tries to decrease the reference count for the extension, which fails
  //   // if the extension has already been reloaded; so instead we post a task.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&extensions::ExtensionRegistrar::ReloadExtension,
                       registrar->GetWeakPtr(), extension_id));
  // }
}

bool DesktopAndroidRuntimeApiDelegate::CheckForUpdates(
    const ExtensionId& extension_id,
    UpdateCheckCallback callback) {
  return false;
}

void DesktopAndroidRuntimeApiDelegate::OpenURL(const GURL& uninstall_url) {
  // TODO(crbug.com/373434594): Support opening URLs.
  NOTIMPLEMENTED();
}

bool DesktopAndroidRuntimeApiDelegate::OpenOptionsPage(
    const Extension* extension,
    content::BrowserContext* browser_context) {
  return extensions::ExtensionTabUtil::OpenOptionsPageFromAPI(extension,
                                                              browser_context);
}

bool DesktopAndroidRuntimeApiDelegate::GetPlatformInfo(
    api::runtime::PlatformInfo* info) {
  info->os = api::runtime::PlatformOs::kAndroid;
  return true;
}

bool DesktopAndroidRuntimeApiDelegate::RestartDevice(
    std::string* error_message) {
  // TODO(crbug.com/373434594): Support device restart.
  NOTIMPLEMENTED();
  *error_message = "not implemented";
  return false;
}

}  // namespace extensions
