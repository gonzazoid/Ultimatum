// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import android.os.Bundle;
import androidx.preference.Preference;
// import androidx.preference.PreferenceFragmentCompat;

// import java.util.function.Supplier;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.user_prefs.UserPrefs;

// import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
// import org.chromium.components.browser_ui.site_settings.BaseSiteSettingsFragment;
// import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
// import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.SettingsFragment;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;

/**
 * Fragment that allows the user to configure AdBlock related preferences.
 */
public class GesturePreferences extends ChromeBaseSettingsFragment
        implements Preference.OnPreferenceChangeListener {
    public static final String PREF_PULL_TO_REFRESH = "pull_to_refresh";

    private final SettableMonotonicObservableSupplier<String> mPageTitle =
            ObservableSuppliers.createMonotonic();

    @Override
    public MonotonicObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public @SettingsFragment.AnimationType int getAnimationType() {
        return SettingsFragment.AnimationType.PROPERTY;
    }

    @Override
    public @Nullable String getMainMenuKey() {
        return "gestures";
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        mPageTitle.set(getString(R.string.prefs_gestures));
        SettingsUtils.addPreferencesFromResource(this, R.xml.gesture_preferences);

        ChromeSwitchPreference pullToRefreshSwitch =
                (ChromeSwitchPreference) findPreference(PREF_PULL_TO_REFRESH);
        pullToRefreshSwitch.setOnPreferenceChangeListener(this); // seems like we don't need this
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        return true;
    }
}
