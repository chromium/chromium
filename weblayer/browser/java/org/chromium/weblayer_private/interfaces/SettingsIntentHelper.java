// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

/**
 * A helper class for creating Intents to start Settings UI Fragments.
 */
public class SettingsIntentHelper {
    private static Bundle createSettingsExtras(String profileName, boolean isIncognito) {
        Bundle extras = new Bundle();
        extras.putString(SettingsFragmentArgs.PROFILE_NAME, profileName);
        extras.putBoolean(SettingsFragmentArgs.IS_INCOGNITO_PROFILE, isIncognito);
        return extras;
    }

    /** Creates an Intent that launches the main category list UI. */
    public static Intent createIntentForSiteSettingsCategoryList(
            Context context, String profileName, boolean isIncognito) {
        Bundle extras = createSettingsExtras(profileName, isIncognito);
        extras.putString(SettingsFragmentArgs.FRAGMENT_NAME, SettingsFragmentArgs.CATEGORY_LIST);
        return createIntentWithExtras(context, extras);
    }

    /** Creates an Intent that launches the settings UI for a single category. */
    public static Intent createIntentForSiteSettingsSingleCategory(Context context,
            String profileName, boolean isIncognito, String categoryType, String categoryTitle) {
        Bundle extras = createSettingsExtras(profileName, isIncognito);
        extras.putString(SettingsFragmentArgs.FRAGMENT_NAME, SettingsFragmentArgs.SINGLE_CATEGORY);

        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(SettingsFragmentArgs.SINGLE_CATEGORY_TYPE, categoryType);
        fragmentArgs.putString(SettingsFragmentArgs.SINGLE_CATEGORY_TITLE, categoryTitle);
        extras.putBundle(SettingsFragmentArgs.FRAGMENT_ARGUMENTS, fragmentArgs);
        return createIntentWithExtras(context, extras);
    }

    /** Creates an Intent that launches the single website settings UI. */
    public static Intent createIntentForSiteSettingsSingleWebsite(
            Context context, String profileName, boolean isIncognito, String url) {
        Bundle extras = createSettingsExtras(profileName, isIncognito);
        extras.putString(SettingsFragmentArgs.FRAGMENT_NAME, SettingsFragmentArgs.SINGLE_WEBSITE);

        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(SettingsFragmentArgs.SINGLE_WEBSITE_URL, url);
        extras.putBundle(SettingsFragmentArgs.FRAGMENT_ARGUMENTS, fragmentArgs);
        return createIntentWithExtras(context, extras);
    }

    /** Creates an Intent that launches the all sites settings UI. */
    public static Intent createIntentForSiteSettingsAllSites(
            Context context, String profileName, boolean isIncognito, String type, String title) {
        Bundle extras = createSettingsExtras(profileName, isIncognito);
        extras.putString(SettingsFragmentArgs.FRAGMENT_NAME, SettingsFragmentArgs.ALL_SITES);

        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(SettingsFragmentArgs.ALL_SITES_TITLE, title);
        fragmentArgs.putString(SettingsFragmentArgs.ALL_SITES_TYPE, type);
        extras.putBundle(SettingsFragmentArgs.FRAGMENT_ARGUMENTS, fragmentArgs);
        return createIntentWithExtras(context, extras);
    }

    public static Intent createIntentForAccessibilitySettings(
            Context context, String profileName, boolean isIncognito) {
        Bundle extras = createSettingsExtras(profileName, isIncognito);
        extras.putString(SettingsFragmentArgs.FRAGMENT_NAME, SettingsFragmentArgs.ACCESSIBILITY);
        return createIntentWithExtras(context, extras);
    }

    private static Intent createIntentWithExtras(Context context, Bundle extras) {
        Intent intent = new Intent();
        intent.setClassName(context, SettingsFragmentArgs.ACTIVITY_CLASS_NAME);
        intent.putExtras(extras);
        return intent;
    }

    private SettingsIntentHelper() {}
}
