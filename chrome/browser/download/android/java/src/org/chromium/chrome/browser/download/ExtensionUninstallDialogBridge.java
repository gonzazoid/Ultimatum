// Copyright 2021 The Chromium Authors
// Copyright 2025 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.download.dialogs.ExtensionUninstallDialog;
import org.chromium.chrome.browser.download.interstitial.NewDownloadTab;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

/**
 * Glues extension uninstall dialogs UI code and handles the communication to uninstall native
 * backend.
 */
public class ExtensionUninstallDialogBridge {
    private long mNativeExtensionUninstallDialogBridge;

    /**
     * Constructor, taking a pointer to the native instance.
     *
     * @param nativeExtensionUninstallDialogBridge Pointer to the native object.
     */
    public ExtensionUninstallDialogBridge(long nativeExtensionUninstallDialogBridge) {
        mNativeExtensionUninstallDialogBridge = nativeExtensionUninstallDialogBridge;
    }

    @CalledByNative
    public static ExtensionUninstallDialogBridge create(long nativeDialog) {
        return new ExtensionUninstallDialogBridge(nativeDialog);
    }

    /**
     * Called to show a warning dialog for extension install.
     *
     * @param windowAndroid Window to show the dialog.
     * @param modalTitle modal title.
     * @param acceptButtonTitle accept button text.
     * @param cancelButtonTitle cancel button text.
     * @param iconId The icon resource for the warning dialog.
     */
    @CalledByNative
    public void showDialog(
            WindowAndroid windowAndroid,
            @JniType("std::u16string") String modalTitle,
            @JniType("std::u16string") String acceptButtonTitle,
            @JniType("std::u16string") String cancelButtonTitle,
            int iconId) {
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            onCancel(windowAndroid);
            return;
        }

        new ExtensionUninstallDialog()
                .show(
                        activity,
                        ((ModalDialogManagerHolder) activity).getModalDialogManager(),
                        modalTitle,
                        acceptButtonTitle,
                        cancelButtonTitle,
                        iconId,
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
        mNativeExtensionUninstallDialogBridge = 0;
    }

    private void onAccepted() {
        ExtensionUninstallDialogBridgeJni.get().accepted(mNativeExtensionUninstallDialogBridge);
    }

    private void onCancel(WindowAndroid windowAndroid) {
        ExtensionUninstallDialogBridgeJni.get()
                .cancelled(mNativeExtensionUninstallDialogBridge);
        NewDownloadTab.closeExistingNewDownloadTab(windowAndroid);
    }

    @NativeMethods
    public interface Natives {
        void accepted(
                long nativeExtensionUninstallDialogBridge);

        void cancelled(
                long nativeExtensionUninstallDialogBridge);
    }
}
