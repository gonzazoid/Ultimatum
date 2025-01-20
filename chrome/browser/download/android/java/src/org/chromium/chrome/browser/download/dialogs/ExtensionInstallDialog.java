// Copyright 2021 The Chromium Authors
// Copyright 2025 gonzazoid
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.content.Context;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;

import androidx.annotation.IntDef;
import androidx.core.content.res.ResourcesCompat;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.download.R;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Dialog for confirming that user want to install an extension, using the default model dialog
 * from ModalDialogManager.
 */
public class ExtensionInstallDialog {
    /**
     * Events related to the extension install dialog, used for UMA reporting.
     * These values are persisted to logs. Entries should not be renumbered and
     * numeric values should never be reused.
     */
    @IntDef({
        ExtensionInstallDialogEvent.EXTENSION_INSTALL_DIALOG_SHOW,
        ExtensionInstallDialogEvent.EXTENSION_INSTALL_DIALOG_CONFIRM,
        ExtensionInstallDialogEvent.EXTENSION_INSTALL_DIALOG_CANCEL,
        ExtensionInstallDialogEvent.EXTENSION_INSTALL_DIALOG_DISMISS
    })
    private @interface ExtensionInstallDialogEvent {
        int EXTENSION_INSTALL_DIALOG_SHOW = 0;
        int EXTENSION_INSTALL_DIALOG_CONFIRM = 1;
        int EXTENSION_INSTALL_DIALOG_CANCEL = 2;
        int EXTENSION_INSTALL_DIALOG_DISMISS = 3;

        int COUNT = 4;
    }

    public ExtensionInstallDialog() {}

    /**
     * Called to show a warning dialog for dangerous download.
     * @param context Context for showing the dialog.
     * @param modalDialogManager Manager for managing the modal dialog.
     * @param modalTitle title.
     * @param acceptButtonTitle text for accept button
     * @param cancelButtonTitle text for cancel button
     * @param iconPngData extension's icon.
     * @param callback Callback to run when confirming the install, true for accept the install,
     *         false otherwise.
     */
    public void show(
            Context context,
            ModalDialogManager modalDialogManager,
            String modalTitle,
            String acceptButtonTitle,
            String cancelButtonTitle,
            byte[] iconPngData,
            Callback<Boolean> callback) {
        var resources = context.getResources();

        var controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, int buttonType) {
                        boolean acceptInstall =
                                buttonType == ModalDialogProperties.ButtonType.POSITIVE;
                        if (callback != null) {
                            callback.onResult(acceptInstall);
                        }
                        modalDialogManager.dismissDialog(
                                model,
                                acceptInstall
                                        ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                        : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                        recordExtensionInstallDialogEvent(
                                acceptInstall
                                        ? ExtensionInstallDialogEvent
                                                .EXTENSION_INSTALL_DIALOG_CONFIRM
                                        : ExtensionInstallDialogEvent
                                                .EXTENSION_INSTALL_DIALOG_CANCEL);
                    }

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {
                        if (dismissalCause != DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                && dismissalCause != DialogDismissalCause.NEGATIVE_BUTTON_CLICKED) {
                            if (callback != null) callback.onResult(false);
                            recordExtensionInstallDialogEvent(
                                    ExtensionInstallDialogEvent.EXTENSION_INSTALL_DIALOG_DISMISS);
                        }
                    }
                };
        PropertyModel propertyModel =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(
                                ModalDialogProperties.TITLE,
                                modalTitle)
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                acceptButtonTitle)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                cancelButtonTitle)
                        .with(
                                ModalDialogProperties.TITLE_ICON,
                                new BitmapDrawable(resources, BitmapFactory.decodeByteArray(iconPngData, 0, iconPngData.length)))
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_OUTLINE)
                        .with(
                                ModalDialogProperties.BUTTON_TAP_PROTECTION_PERIOD_MS,
                                UiUtils.PROMPT_INPUT_PROTECTION_SHORT_DELAY_MS)
                        .build();

        modalDialogManager.showDialog(propertyModel, ModalDialogManager.ModalDialogType.TAB);
        recordExtensionInstallDialogEvent(
                ExtensionInstallDialogEvent.EXTENSION_INSTALL_DIALOG_SHOW);
    }

    /**
     * Collects extension install dialog UI event metrics.
     *
     * @param event The UI event to collect.
     */
    private static void recordExtensionInstallDialogEvent(
            @ExtensionInstallDialogEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "Download.ExtensionInstallDialog.Events", event, ExtensionInstallDialogEvent.COUNT);
    }
}
