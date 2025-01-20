// Copyright 2021 The Chromium Authors
// Copyright 2025 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/extension_uninstall_dialog_bridge.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
// #include "base/containers/contains.h"
#include "base/files/file_path.h"
// #include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/grit/generated_resources.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

#include "extensions/browser/ui_util.h"
#include "extensions/common/extension.h"
#include "chrome/browser/extensions/extension_util.h"
#include "components/strings/grit/components_strings.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/download/android/jni_headers/ExtensionUninstallDialogBridge_jni.h"

using base::android::ConvertJavaStringToUTF8;
// using base::android::JavaParamRef;

ExtensionUninstallDialogBridge::ExtensionUninstallDialogBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_ExtensionUninstallDialogBridge_create(
      env, reinterpret_cast<intptr_t>(this)));
}

ExtensionUninstallDialogBridge::~ExtensionUninstallDialogBridge() {
  Java_ExtensionUninstallDialogBridge_destroy(
      base::android::AttachCurrentThread(), java_object_);
}

void ExtensionUninstallDialogBridge::Show(const extensions::Extension* extension,
                                         ui::WindowAndroid* window_android,
                                         base::OnceCallback<void()> accepted,
                                         base::OnceCallback<void()> canceled) {
  accepted_ = std::move(accepted);
  canceled_ = std::move(canceled);

  if (!window_android) {
    return;
  }

  JNIEnv* env = base::android::AttachCurrentThread();

  auto title = l10n_util::GetStringFUTF16(
          IDS_EXTENSION_PROMPT_UNINSTALL_TITLE,
          extensions::ui_util::GetFixupExtensionNameForUIDisplay(
              extension->name()));
  Java_ExtensionUninstallDialogBridge_showDialog(
      env, java_object_, window_android->GetJavaObject(),
      title,
      l10n_util::GetStringUTF16(
                  IDS_EXTENSION_PROMPT_UNINSTALL_BUTTON),
      l10n_util::GetStringUTF16(IDS_CANCEL),
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_INFOBAR_WARNING));
}

void ExtensionUninstallDialogBridge::Accepted(JNIEnv* env) {
  canceled_.Reset();
  if(!accepted_.is_null()) {
    std::move(accepted_).Run();
  }
}

void ExtensionUninstallDialogBridge::Cancelled(JNIEnv* env) {
  accepted_.Reset();
  if(!canceled_.is_null()) {
    std::move(canceled_).Run();
  }
}

DEFINE_JNI(ExtensionUninstallDialogBridge)
