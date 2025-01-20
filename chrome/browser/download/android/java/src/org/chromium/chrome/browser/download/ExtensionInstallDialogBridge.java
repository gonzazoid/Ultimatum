// Copyright 2021 The Chromium Authors
// Copyright 2025 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.download.dialogs.ExtensionInstallDialog;
import org.chromium.chrome.browser.download.interstitial.NewDownloadTab;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

/**
 * Glues extension install dialogs UI code and handles the communication to install native
 * backend.
 */
public class ExtensionInstallDialogBridge {
    private long mNativeExtensionInstallDialogBridge;

    /**
     * Constructor, taking a pointer to the native instance.
     *
     * @param nativeExtensionInstallDialogBridge Pointer to the native object.
     */
    public ExtensionInstallDialogBridge(long nativeExtensionInstallDialogBridge) {
        mNativeExtensionInstallDialogBridge = nativeExtensionInstallDialogBridge;
    }

    @CalledByNative
    public static ExtensionInstallDialogBridge create(long nativeDialog) {
        return new ExtensionInstallDialogBridge(nativeDialog);
    }

    /**
     * Called to show a warning dialog for extension install.
     *
     * @param windowAndroid Window to show the dialog.
     * @param modalTitle modal title.
     * @param acceptButtonTitle accept button text.
     * @param cancelButtonTitle cancel button text.
     * @param iconPngData extension's icon.
     */
    @CalledByNative
    public void showDialog(
            WindowAndroid windowAndroid,
            @JniType("std::u16string") String modalTitle,
            @JniType("std::u16string") String acceptButtonTitle,
            @JniType("std::u16string") String cancelButtonTitle,
            byte[] iconPngData) {
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            onCancel(windowAndroid);
            return;
        }

        new ExtensionInstallDialog()
                .show(
                        activity,
                        ((ModalDialogManagerHolder) activity).getModalDialogManager(),
                        modalTitle,
                        acceptButtonTitle,
                        cancelButtonTitle,
                        iconPngData,
                        (accepted) -> {
                            if (accepted) {
                                onAccepted();
                            } else {
                                onCancel(windowAndroid);
                            }
                        });
    }

    @CalledByNative
    private void destroy() {
        mNativeExtensionInstallDialogBridge = 0;
    }

    private void onAccepted() {
        ExtensionInstallDialogBridgeJni.get().accepted(mNativeExtensionInstallDialogBridge);
    }

    private void onCancel(WindowAndroid windowAndroid) {
        ExtensionInstallDialogBridgeJni.get()
                .cancelled(mNativeExtensionInstallDialogBridge);
        NewDownloadTab.closeExistingNewDownloadTab(windowAndroid);
    }

    @NativeMethods
    public interface Natives {
        void accepted(long nativeExtensionInstallDialogBridge);

        void cancelled(long nativeExtensionInstallDialogBridge);
    }
}
