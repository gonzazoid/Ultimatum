// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.dialogs;

import android.content.Context;

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
 * Dialog for confirming that user want to uninstall an extension, using the default model dialog
 * from ModalDialogManager.
 */
public class ExtensionUninstallDialog {
    /**
     * Events related to the extension uninstall dialog, used for UMA reporting.
     * These values are persisted to logs. Entries should not be renumbered and
     * numeric values should never be reused.
     */
    @IntDef({
        ExtensionUninstallDialogEvent.EXTENSION_UNINSTALL_DIALOG_SHOW,
        ExtensionUninstallDialogEvent.EXTENSION_UNINSTALL_DIALOG_CONFIRM,
        ExtensionUninstallDialogEvent.EXTENSION_UNINSTALL_DIALOG_CANCEL,
        ExtensionUninstallDialogEvent.EXTENSION_UNINSTALL_DIALOG_DISMISS
    })
    private @interface ExtensionUninstallDialogEvent {
        int EXTENSION_UNINSTALL_DIALOG_SHOW = 0;
        int EXTENSION_UNINSTALL_DIALOG_CONFIRM = 1;
        int EXTENSION_UNINSTALL_DIALOG_CANCEL = 2;
        int EXTENSION_UNINSTALL_DIALOG_DISMISS = 3;

        int COUNT = 4;
    }

    public ExtensionUninstallDialog() {}

    /**
     * Called to show a warning dialog for dangerous download.
     * @param context Context for showing the dialog.
     * @param modalDialogManager Manager for managing the modal dialog.
     * @param modalTitle title.
     * @param acceptButtonTitle text for accept button
     * @param cancelButtonTitle text for cancel button
     * @param iconId Icon ID of the warning dialog.
     * @param callback Callback to run when confirming the download, true for accept the download,
     *         false otherwise.
     */
    public void show(
            Context context,
            ModalDialogManager modalDialogManager,
            String modalTitle,
            String acceptButtonTitle,
            String cancelButtonTitle,
            int iconId,
            Callback<Boolean> callback) {
        var resources = context.getResources();

        var controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onClick(PropertyModel model, int buttonType) {
                        boolean acceptUninstall =
                                buttonType == ModalDialogProperties.ButtonType.POSITIVE;
                        if (callback != null) {
                            callback.onResult(acceptUninstall);
                        }
                        modalDialogManager.dismissDialog(
                                model,
                                acceptUninstall
                                        ? DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                        : DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                        recordExtensionUninstallDialogEvent(
                                acceptUninstall
                                        ? ExtensionUninstallDialogEvent
                                                .EXTENSION_UNINSTALL_DIALOG_CONFIRM
                                        : ExtensionUninstallDialogEvent
                                                .EXTENSION_UNINSTALL_DIALOG_CANCEL);
                    }

                    @Override
                    public void onDismiss(PropertyModel model, int dismissalCause) {
                        if (dismissalCause != DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                && dismissalCause != DialogDismissalCause.NEGATIVE_BUTTON_CLICKED) {
                            if (callback != null) callback.onResult(false);
                            recordExtensionUninstallDialogEvent(
                                    ExtensionUninstallDialogEvent.EXTENSION_UNINSTALL_DIALOG_DISMISS);
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
                                ResourcesCompat.getDrawable(resources, iconId, context.getTheme()))
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_OUTLINE_NEGATIVE_OUTLINE)
                        .with(
                                ModalDialogProperties.BUTTON_TAP_PROTECTION_PERIOD_MS,
                                UiUtils.PROMPT_INPUT_PROTECTION_SHORT_DELAY_MS)
                        .build();

        modalDialogManager.showDialog(propertyModel, ModalDialogManager.ModalDialogType.TAB);
        recordExtensionUninstallDialogEvent(
                ExtensionUninstallDialogEvent.EXTENSION_UNINSTALL_DIALOG_SHOW);
    }

    /**
     * Collects extension uninstall dialog UI event metrics.
     *
     * @param event The UI event to collect.
     */
    private static void recordExtensionUninstallDialogEvent(
            @ExtensionUninstallDialogEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "Download.ExtensionUninstallDialog.Events", event, ExtensionUninstallDialogEvent.COUNT);
    }
}
