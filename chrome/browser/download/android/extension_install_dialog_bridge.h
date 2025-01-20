// Copyright 2021 The Chromium Authors
// Copyright 2025 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_EXTENSION_INSTALL_DIALOG_BRIDGE_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_EXTENSION_INSTALL_DIALOG_BRIDGE_H_

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_install_prompt.h"
#include "ui/gfx/native_widget_types.h"

// Class for showing dialogs to asks whether user wants to install an extension
class ExtensionInstallDialogBridge {
 public:
  ExtensionInstallDialogBridge();
  ExtensionInstallDialogBridge(const ExtensionInstallDialogBridge&) = delete;
  ExtensionInstallDialogBridge& operator=(
      const ExtensionInstallDialogBridge&) = delete;

  ~ExtensionInstallDialogBridge();

  // Called to create and show a dialog extension install.
  void Show(std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt,
            ui::WindowAndroid* window_android,
            base::OnceCallback<void()> accepted,
            base::OnceCallback<void()> canceled);

  // Called from Java via JNI.
  void Accepted(JNIEnv* env);

  // Called from Java via JNI.
  void Cancelled(JNIEnv* env);

 private:
  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  base::OnceCallback<void()> accepted_;
  base::OnceCallback<void()> canceled_;
};

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_EXTENSION_INSTALL_DIALOG_BRIDGE_H_
