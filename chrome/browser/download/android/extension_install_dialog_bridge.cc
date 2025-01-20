// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/extension_install_dialog_bridge.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/jni_array.h"
// #include "base/containers/contains.h"
#include "base/files/file_path.h"
// #include "base/ranges/algorithm.h"
#include "base/strings/utf_string_conversions.h"
#include "base/memory/ref_counted_memory.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/grit/generated_resources.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"

#include "extensions/common/extension.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/download/android/jni_headers/ExtensionInstallDialogBridge_jni.h"

using base::android::ConvertJavaStringToUTF8;
// using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

ExtensionInstallDialogBridge::ExtensionInstallDialogBridge() {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_ExtensionInstallDialogBridge_create(
      env, reinterpret_cast<intptr_t>(this)));
}

ExtensionInstallDialogBridge::~ExtensionInstallDialogBridge() {
  Java_ExtensionInstallDialogBridge_destroy(
      base::android::AttachCurrentThread(), java_object_);
}

void ExtensionInstallDialogBridge::Show(std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt,
                                         ui::WindowAndroid* window_android,
                                         base::OnceCallback<void()> accepted,
                                         base::OnceCallback<void()> canceled) {
  if (!window_android) {
    return;
  }

  accepted_ = std::move(accepted);
  canceled_ = std::move(canceled);

  JNIEnv* env = base::android::AttachCurrentThread();

  auto icon_png_data = prompt->icon().As1xPNGBytes();
  ScopedJavaLocalRef<jbyteArray> j_icon =
      base::android::ToJavaByteArray(env, *icon_png_data);

  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt_helper =
    std::make_unique<ExtensionInstallPrompt::Prompt>(ExtensionInstallPrompt::INSTALL_PROMPT);
  prompt_helper->set_extension(prompt->extension());

  Java_ExtensionInstallDialogBridge_showDialog(
      env, java_object_, window_android->GetJavaObject(),
      prompt_helper->GetDialogTitle(),
      prompt_helper->GetAcceptButtonLabel(),
      prompt_helper->GetAbortButtonLabel(),
      j_icon);
}

void ExtensionInstallDialogBridge::Accepted(JNIEnv* env) {
  std::move(accepted_).Run();
}

void ExtensionInstallDialogBridge::Cancelled(JNIEnv* env) {
  std::move(canceled_).Run();
}

DEFINE_JNI(ExtensionInstallDialogBridge)
