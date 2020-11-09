// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

/**
 * A helper class for creating Intents to start the Site Settings UI.
 */
public class SiteSettingsIntentHelper {
    private static Bundle createSiteSettingsExtras(String profileName, boolean isIncognito) {
        Bundle extras = new Bundle();
        extras.putString(SiteSettingsFragmentArgs.PROFILE_NAME, profileName);
        extras.putBoolean(SiteSettingsFragmentArgs.IS_INCOGNITO_PROFILE, isIncognito);
        return extras;
    }

    /** Creates an Intent that launches the main category list UI. */
    public static Intent createIntentForCategoryList(
            Context context, String profileName, boolean isIncognito) {
        Bundle extras = createSiteSettingsExtras(profileName, isIncognito);
        extras.putString(
                SiteSettingsFragmentArgs.FRAGMENT_NAME, SiteSettingsFragmentArgs.CATEGORY_LIST);
        return createIntentWithExtras(context, extras);
    }

    /** Creates an Intent that launches the settings UI for a single category. */
    public static Intent createIntentForSingleCategory(Context context, String profileName,
            boolean isIncognito, String categoryType, String categoryTitle) {
        Bundle extras = createSiteSettingsExtras(profileName, isIncognito);
        extras.putString(
                SiteSettingsFragmentArgs.FRAGMENT_NAME, SiteSettingsFragmentArgs.SINGLE_CATEGORY);

        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(SiteSettingsFragmentArgs.SINGLE_CATEGORY_TYPE, categoryType);
        fragmentArgs.putString(SiteSettingsFragmentArgs.SINGLE_CATEGORY_TITLE, categoryTitle);
        extras.putBundle(SiteSettingsFragmentArgs.FRAGMENT_ARGUMENTS, fragmentArgs);
        return createIntentWithExtras(context, extras);
    }

    /** Creates an Intent that launches the single website settings UI. */
    public static Intent createIntentForSingleWebsite(
            Context context, String profileName, boolean isIncognito, String url) {
        Bundle extras = createSiteSettingsExtras(profileName, isIncognito);
        extras.putString(
                SiteSettingsFragmentArgs.FRAGMENT_NAME, SiteSettingsFragmentArgs.SINGLE_WEBSITE);

        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(SiteSettingsFragmentArgs.SINGLE_WEBSITE_URL, url);
        extras.putBundle(SiteSettingsFragmentArgs.FRAGMENT_ARGUMENTS, fragmentArgs);
        return createIntentWithExtras(context, extras);
    }

    /** Creates an Intent that launches the all sites settings UI. */
    public static Intent createIntentForAllSites(
            Context context, String profileName, boolean isIncognito, String type, String title) {
        Bundle extras = createSiteSettingsExtras(profileName, isIncognito);
        extras.putString(
                SiteSettingsFragmentArgs.FRAGMENT_NAME, SiteSettingsFragmentArgs.ALL_SITES);

        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(SiteSettingsFragmentArgs.ALL_SITES_TITLE, title);
        fragmentArgs.putString(SiteSettingsFragmentArgs.ALL_SITES_TYPE, type);
        extras.putBundle(SiteSettingsFragmentArgs.FRAGMENT_ARGUMENTS, fragmentArgs);
        return createIntentWithExtras(context, extras);
    }

    private static Intent createIntentWithExtras(Context context, Bundle extras) {
        Intent intent = new Intent();
        intent.setClassName(context, SiteSettingsFragmentArgs.ACTIVITY_CLASS_NAME);
        intent.putExtras(extras);
        return intent;
    }

    private SiteSettingsIntentHelper() {}
}
