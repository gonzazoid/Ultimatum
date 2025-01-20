// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;

import org.chromium.base.Log;

import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.extensions.ContextMenuSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.extensions.ExtensionAction;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionContextMenuBridge;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionsBridge;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.listmenu.ListMenuButton;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionPopupContents;
import org.chromium.chrome.browser.ui.extensions.ExtensionActionsBridge;
import org.chromium.extensions.ShowAction;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.RectProvider;

/**
 * Mediator for the extensions menu. This class is responsible for listening to changes in the
 * extensions and updating the model accordingly.
 */
@NullMarked
class ExtensionsMenuMediator implements Destroyable {
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final ListMenuButton mExtensionsMenuButton;
    private final ActionsUpdateDelegate mActionsUpdateDelegate = new ActionsUpdateDelegate();
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final Runnable mOnUpdateFinishedRunnable;
    private final Callback<Boolean> mOnExtensionsAvailableCallback;
    private final ExtensionActionsUpdateHelper mExtensionActionsUpdateHelper;
    private final Callback<Profile> mProfileUpdatedCallback = this::onProfileUpdated;
    private final View mRootView;

    @Nullable private Profile mProfile;
    @Nullable private ExtensionActionPopup mCurrentPopup;

    public ExtensionsMenuMediator(
            Context context,
            WindowAndroid windowAndroid,
            ListMenuButton extensionsMenuButton,
            ObservableSupplier<Profile> profileSupplier,
            ObservableSupplier<Tab> currentTabSupplier,
            ModelList extensionModels,
            Runnable onUpdateFinishedRunnable,
            Callback<Boolean> onExtensionsAvailableCallback,
            View rootView) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mExtensionsMenuButton = extensionsMenuButton;
        mProfileSupplier = profileSupplier;
        mProfileSupplier.addObserver(mProfileUpdatedCallback);

        mOnUpdateFinishedRunnable = onUpdateFinishedRunnable;
        mOnExtensionsAvailableCallback = onExtensionsAvailableCallback;
        mRootView = rootView;

        mExtensionActionsUpdateHelper =
                new ExtensionActionsUpdateHelper(
                        extensionModels,
                        profileSupplier,
                        currentTabSupplier,
                        mActionsUpdateDelegate);
    }

    private static class RelativeViewRectProvider extends RectProvider {
        private final View mAnchorView;
        private final View mParentView;

        RelativeViewRectProvider(View anchorView, View parentView) {
            mAnchorView = anchorView;
            mParentView = parentView;
        }

        /**
         * For {@link AnchoredPopupWindow} to correctly place nested popup windows, we have to make
         * sure to send coordinates relative to the main window of the application, not positions
         * relative to the parent popup window nor the screen.
         */
        @Override
        public Rect getRect() {
            int[] anchorLocation = new int[2];
            mAnchorView.getLocationOnScreen(anchorLocation);

            int[] parentLocation = new int[2];
            mParentView.getLocationOnScreen(parentLocation);

            int x = anchorLocation[0] - parentLocation[0];
            int y = anchorLocation[1] - parentLocation[1];

            return new Rect(x, y, x + mAnchorView.getWidth(), y + mAnchorView.getHeight());
        }
    }

    private void onProfileUpdated(@Nullable Profile profile) {
        if (profile == mProfile) {
            return;
        }

        mProfile = profile;

        // TODO(crbug.com/422307625): Remove this check once extensions are ready for dogfooding.
        boolean extensionsSupported = true;
        if (mProfile != null) {
            ExtensionActionsBridge extensionActionsBridge = ExtensionActionsBridge.get(mProfile);
            if (extensionActionsBridge != null && extensionActionsBridge.extensionsEnabled()) {
                extensionsSupported = true;
            }
        }
        mOnExtensionsAvailableCallback.onResult(extensionsSupported);
    }

    @Override
    public void destroy() {
        mExtensionActionsUpdateHelper.destroy();
        mProfileSupplier.removeObserver(mProfileUpdatedCallback);
    }
/*
    private void onPrimaryClick(ListMenuButton buttonView, String actionId) {
        Tab currentTab = mExtensionActionsUpdateHelper.getCurrentTab();
        if (currentTab == null) {
            return;
        }

        WebContents webContents = currentTab.getWebContents();
        Profile profile = mExtensionActionsUpdateHelper.getProfile();
        if (webContents == null || profile == null) {
            return;
        }

        ExtensionActionContextMenuBridge bridge =
                new ExtensionActionContextMenuBridge(
                        profile, actionId, webContents, ContextMenuSource.MENU_ITEM);

        ExtensionActionContextMenuUtils.showContextMenu(
                mContext,
                buttonView,
                bridge,
                new RelativeViewRectProvider(buttonView, mRootView),
                mRootView);
    }
*/
    private void onPrimaryClick(String actionId) {
      Log.i("ULTIMATUM", "EXTENSION: " + actionId);
       ExtensionActionsBridge extensionActionsBridge =
                mExtensionActionsUpdateHelper.getExtensionActionsBridge();
        Tab currentTab = mExtensionActionsUpdateHelper.getCurrentTab();
        if (extensionActionsBridge == null || currentTab == null) {
            return;
        }

        WebContents webContents = currentTab.getWebContents();
        if (webContents == null) {
            // TODO(crbug.com/385985177): Revisit how to handle this case.
            return;
        }

        @ShowAction
        int showAction =
                extensionActionsBridge.runAction(actionId, currentTab.getId(), webContents);
        switch (showAction) {
            case ShowAction.NONE:
                break;
            case ShowAction.SHOW_POPUP:
                openPopup(actionId);
                break;
            case ShowAction.TOGGLE_SIDE_PANEL:
                // Log.e(TAG, "Extension side panels are not implemented yet");
                break;
        }
    }

    private void openPopup(String actionId) {
        // TODO(crbug.com/385987224): Do not open a popup again when the user clicks the action
        // button while its popup is open.
        // or we can just close popup if the user clicks the extension icon again
        closePopup();

        Tab currentTab = mExtensionActionsUpdateHelper.getCurrentTab();
        Profile profile = mExtensionActionsUpdateHelper.getProfile();

        if (profile == null || currentTab == null) {
            return;
        }
        int tabId = currentTab.getId();

        ExtensionActionPopupContents contents =
                ExtensionActionPopupContents.create(profile, actionId, tabId);
        assert mCurrentPopup == null;
        mCurrentPopup =
                new ExtensionActionPopup(mContext, mWindowAndroid, mExtensionsMenuButton, actionId, contents);
        mCurrentPopup.loadInitialPage();
        mCurrentPopup.addOnDismissListener(this::closePopup);
    }

    private void closePopup() {
        if (mCurrentPopup == null) {
            return;
        }
        assert mExtensionActionsUpdateHelper.getExtensionActionsBridge() != null;

        // Clear mCurrentPopup now to avoid calling closePopup recursively via OnDismissListener.
        ExtensionActionPopup popup = mCurrentPopup;
        mCurrentPopup = null;
        popup.destroy();
    }

    private class ActionsUpdateDelegate
            implements ExtensionActionsUpdateHelper.ActionsUpdateDelegate {
        @Override
        public void onUpdateStarted() {}

        @Override
        public ListItem createActionModel(
                ExtensionActionsBridge extensionActionsBridge, int tabId, String actionId) {
            ExtensionAction action = extensionActionsBridge.getAction(actionId, tabId);
            assert action != null;

            Tab currentTab = mExtensionActionsUpdateHelper.getCurrentTab();
            WebContents webContents = currentTab == null ? null : currentTab.getWebContents();

            Bitmap icon =
                    ExtensionActionIconUtil.getActionIcon(
                            mContext, extensionActionsBridge, actionId, tabId, webContents);
            assert icon != null;
            return new ListItem(
                    0,
                    new PropertyModel.Builder(ExtensionsMenuItemProperties.ALL_KEYS)
                            .with(ExtensionsMenuItemProperties.TITLE, action.getTitle())
                            .with(ExtensionsMenuItemProperties.ICON, icon)
                            .with(
                                    ExtensionsMenuItemProperties.CLICK_LISTENER,
                                    (view) -> onPrimaryClick(/* (ListMenuButton) view, */ actionId))
                            .build());
        }

        @Override
        public void onUpdateFinished() {
            mOnUpdateFinishedRunnable.run();
        }
    }
}
