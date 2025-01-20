// Copyright 2021 The Chromium Authors
// Copyright 2025 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_EXTENSION_UNINSTALL_DIALOG_BRIDGE_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_EXTENSION_UNINSTALL_DIALOG_BRIDGE_H_

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/extension_install_prompt.h"

// Class for showing dialogs to asks whether user wants to uninstall an extension
class ExtensionUninstallDialogBridge {
 public:
  ExtensionUninstallDialogBridge();
  ExtensionUninstallDialogBridge(const ExtensionUninstallDialogBridge&) = delete;
  ExtensionUninstallDialogBridge& operator=(
      const ExtensionUninstallDialogBridge&) = delete;

  ~ExtensionUninstallDialogBridge();

  // Called to create and show a dialog extension uninstall.
  void Show(const extensions::Extension* extension,
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

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_EXTENSION_UNINSTALL_DIALOG_BRIDGE_H_
