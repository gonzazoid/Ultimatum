// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.settings;

import android.content.Context;
import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.ResolveInfo;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.net.Uri;
import android.text.TextUtils;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

import java.util.HexFormat;
import java.io.UnsupportedEncodingException;

import androidx.preference.Preference;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.download.DownloadDialogBridge;
import org.chromium.chrome.browser.download.DownloadDirectoryProvider;
import org.chromium.chrome.browser.download.DownloadPromptStatus;
import org.chromium.chrome.browser.download.MimeUtils;
import org.chromium.chrome.browser.download.R;
import org.chromium.chrome.browser.pdf.PdfUtils;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.search.ChromeBaseSearchIndexProvider;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.user_prefs.UserPrefs;

// import org.chromium.components.browser_ui.settings.ChromeBaseCheckBoxPreference;

/** Fragment containing Download settings. */
@NullMarked
public class DownloadSettings extends ChromeBaseSettingsFragment
        implements Preference.OnPreferenceChangeListener {
    public static final String PREF_LOCATION_CHANGE = "location_change";
    public static final String PREF_LOCATION_PROMPT_ENABLED = "location_prompt_enabled";
    public static final String PREF_AUTO_OPEN_PDF_ENABLED = "auto_open_pdf_enabled";

    private DownloadLocationPreference mLocationChangePref;
    private ChromeSwitchPreference mLocationPromptEnabledPref;
    private ManagedPreferenceDelegate mLocationPromptEnabledPrefDelegate;
    private ChromeSwitchPreference mAutoOpenPdfEnabledPref;
    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();

    private ChromeSwitchPreference mExternalDownloadManager;

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String s) {
        mPageTitle.set(getString(R.string.menu_downloads));
        SettingsUtils.addPreferencesFromResource(this, R.xml.download_preferences);

        mLocationPromptEnabledPref =
                (ChromeSwitchPreference) findPreference(PREF_LOCATION_PROMPT_ENABLED);
        mLocationPromptEnabledPrefDelegate =
                new ChromeManagedPreferenceDelegate(getProfile()) {
                    @Override
                    public boolean isPreferenceControlledByPolicy(Preference preference) {
                        return DownloadDialogBridge.isLocationDialogManaged(getProfile());
                    }
                };
        mLocationPromptEnabledPref.setManagedPreferenceDelegate(mLocationPromptEnabledPrefDelegate);
        if (shouldEnableLocationPromptPref(getProfile())) {
            mLocationPromptEnabledPref.setVisible(false);
        } else {
            mLocationPromptEnabledPref.setOnPreferenceChangeListener(this);
        }

        mLocationChangePref = (DownloadLocationPreference) findPreference(PREF_LOCATION_CHANGE);
        mLocationChangePref.setDownloadLocationHelper(new DownloadLocationHelperImpl(getProfile()));

        mAutoOpenPdfEnabledPref =
                (ChromeSwitchPreference) findPreference(PREF_AUTO_OPEN_PDF_ENABLED);
        if (shouldEnableAutoOpenPdf(getProfile())) {
            mAutoOpenPdfEnabledPref.setVisible(false);
        } else {
            mAutoOpenPdfEnabledPref.setOnPreferenceChangeListener(this);
            String summary =
                    (MimeUtils.getPdfIntentHandlers().size() == 1)
                            ? getActivity()
                                    .getString(
                                            R.string.auto_open_pdf_enabled_with_app_description,
                                            MimeUtils.getDefaultPdfViewerName())
                            : getActivity().getString(R.string.auto_open_pdf_enabled_description);
            mAutoOpenPdfEnabledPref.setSummaryOn(summary);
        }

        mExternalDownloadManager = (ChromeSwitchPreference) findPreference("enable_external_download_manager");
        mExternalDownloadManager.setOnPreferenceChangeListener(this);
    }

    private static boolean shouldEnableLocationPromptPref(Profile profile) {
        return PdfUtils.shouldOpenPdfInline(profile.isOffTheRecord())
                && DownloadDirectoryProvider.getSecondaryStorageDownloadDirectories().isEmpty();
    }

    private static boolean shouldEnableAutoOpenPdf(Profile profile) {
        return PdfUtils.shouldOpenPdfInline(profile.isOffTheRecord());
    }

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onDisplayPreferenceDialog(Preference preference) {
        if (preference instanceof DownloadLocationPreference) {
            DownloadLocationPreferenceDialog dialogFragment =
                    DownloadLocationPreferenceDialog.newInstance(
                            (DownloadLocationPreference) preference);
            dialogFragment.setTargetFragment(this, 0);
            dialogFragment.show(getParentFragmentManager(), DownloadLocationPreferenceDialog.TAG);
        } else {
            super.onDisplayPreferenceDialog(preference);
        }
    }

    @Override
    public void onStart() {
        super.onStart();
        updateDownloadSettings();
    }

    private void updateDownloadSettings() {
        mLocationChangePref.updateSummary();

        if (DownloadDialogBridge.isLocationDialogManaged(getProfile())) {
            // Location prompt can be controlled by the enterprise policy.
            mLocationPromptEnabledPref.setChecked(
                    DownloadDialogBridge.getPromptForDownloadPolicy(getProfile()));
        } else {
            // Location prompt is marked enabled if the prompt status is not DONT_SHOW.
            boolean isLocationPromptEnabled =
                    DownloadDialogBridge.getPromptForDownloadAndroid(getProfile())
                            != DownloadPromptStatus.DONT_SHOW;
            mLocationPromptEnabledPref.setChecked(isLocationPromptEnabled);
            mLocationPromptEnabledPref.setEnabled(true);
        }
        if (!PdfUtils.shouldOpenPdfInline(getProfile().isOffTheRecord())) {
            mAutoOpenPdfEnabledPref.setChecked(
                    UserPrefs.get(getProfile()).getBoolean(Pref.AUTO_OPEN_PDF_ENABLED));
            mAutoOpenPdfEnabledPref.setEnabled(true);
        }

        if (mExternalDownloadManager != null) {
            mExternalDownloadManager.setChecked(ContextUtils.getAppSharedPreferences().getBoolean("enable_external_download_manager", false));
            if (ContextUtils.getAppSharedPreferences().getBoolean("enable_external_download_manager", false)
                  && !TextUtils.isEmpty(ContextUtils.getAppSharedPreferences().getString("selected_external_download_manager_package_name", ""))) {
                mExternalDownloadManager.setSummary(ContextUtils.getAppSharedPreferences().getString("selected_external_download_manager_package_name", ""));
            }
        }
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data) {
            if (requestCode == 4242 && resultCode == Activity.RESULT_OK && data != null) {
                 ComponentName componentName = data.getComponent();
                 final String packageName = componentName.getPackageName();
                 final String activityName = componentName.getClassName();
                 SharedPreferences.Editor sharedPreferencesEditor = ContextUtils.getAppSharedPreferences().edit();
                 sharedPreferencesEditor.putString("selected_external_download_manager_package_name", packageName);
                 sharedPreferencesEditor.putString("selected_external_download_manager_activity_name", activityName);
                 sharedPreferencesEditor.apply();
                 updateDownloadSettings();
            }
    }

    // Preference.OnPreferenceChangeListener implementation.
    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (PREF_LOCATION_PROMPT_ENABLED.equals(preference.getKey())) {
            if ((boolean) newValue) {
                // Only update if the download location dialog has been shown before.
                if (DownloadDialogBridge.getPromptForDownloadAndroid(getProfile())
                        != DownloadPromptStatus.SHOW_INITIAL) {
                    DownloadDialogBridge.setPromptForDownloadAndroid(
                            getProfile(), DownloadPromptStatus.SHOW_PREFERENCE);
                }
            } else {
                DownloadDialogBridge.setPromptForDownloadAndroid(
                        getProfile(), DownloadPromptStatus.DONT_SHOW);
            }
        } else if (PREF_AUTO_OPEN_PDF_ENABLED.equals(preference.getKey())) {
            UserPrefs.get(getProfile()).setBoolean(Pref.AUTO_OPEN_PDF_ENABLED, (boolean) newValue);
        } else if ("enable_external_download_manager".equals(preference.getKey())) {
            SharedPreferences.Editor sharedPreferencesEditor = ContextUtils.getAppSharedPreferences().edit();
            sharedPreferencesEditor.putBoolean("enable_external_download_manager", (boolean)newValue);
            sharedPreferencesEditor.apply();
            if ((boolean)newValue == true) {
                    // List<Intent> targetedShareIntents = new ArrayList<Intent>();
                    Intent shareIntent = new Intent(android.content.Intent.ACTION_VIEW, Uri.parse("http://test.com/file.rar"));
                    // Set title and text to share when the user selects an option.
                    shareIntent.putExtra(Intent.EXTRA_TEXT, "http://test.com/file.rar");
                    List<ResolveInfo> resInfo = getActivity().getPackageManager().queryIntentActivities(shareIntent, 0);
                    if (!resInfo.isEmpty()) {
                        // String packageName = ContextUtils.getApplicationContext().getPackageName();
                        // for (ResolveInfo info : resInfo) {
                        //     if (!packageName.equals(info.activityInfo.packageName)) {
                        //         Intent targetedShare = new Intent(android.content.Intent.ACTION_VIEW);
                        //         targetedShare.setPackage(info.activityInfo.packageName.toLowerCase(Locale.ROOT));
                        //         targetedShareIntents.add(targetedShare);
                        //     }
                        // }
                        // Then show the ACTION_PICK_ACTIVITY to let the user select it
                        Intent intentPick = new Intent();
                        intentPick.setAction(Intent.ACTION_PICK_ACTIVITY);
                        // Set the title of the dialog
                        intentPick.putExtra(Intent.EXTRA_TITLE, "Select download manager");
                        intentPick.putExtra(Intent.EXTRA_INTENT, shareIntent);
                        // intentPick.putExtra(Intent.EXTRA_INITIAL_INTENTS, targetedShareIntents.toArray());

                        // Call StartActivityForResult so we can get the app name selected by the user
                        this.startActivityForResult(intentPick, /* REQUEST_CODE_MY_PICK */ 4242);
                    }
            }
        }
        return true;
    }

    public ManagedPreferenceDelegate getLocationPromptEnabledPrefDelegateForTesting() {
        return mLocationPromptEnabledPrefDelegate;
    }

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }

    @Override
    public @Nullable String getMainMenuKey() {
        return "downloads";
    }

    public static final ChromeBaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new ChromeBaseSearchIndexProvider(
                    DownloadSettings.class.getName(), R.xml.download_preferences) {

                @Override
                public void updateDynamicPreferences(
                        Context context, SettingsIndexData indexData, Profile profile) {
                    if (shouldEnableLocationPromptPref(profile)) {
                        indexData.removeEntry(getUniqueId(PREF_LOCATION_PROMPT_ENABLED));
                    }
                    if (shouldEnableAutoOpenPdf(profile)) {
                        indexData.removeEntry(getUniqueId(PREF_AUTO_OPEN_PDF_ENABLED));
                    }
                }
            };
}
