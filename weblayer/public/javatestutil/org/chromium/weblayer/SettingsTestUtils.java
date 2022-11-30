// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.content.Intent;

import org.chromium.weblayer_private.interfaces.SettingsIntentHelper;

/**
 * Helpers for writing tests for the Settings UI.
 */
public final class SettingsTestUtils {
    // This can be removed if/when we move this into SettingsActivity. Currently no embedders
    // need to launch the single site settings UI directly.
    public static Intent createIntentForSiteSettingsSingleWebsite(
            Context context, String profileName, boolean isIncognito, String url) {
        return SettingsIntentHelper.createIntentForSiteSettingsSingleWebsite(
                context, profileName, isIncognito, url);
    }

    // This can be removed if/when we move this into SettingsActivity. Currently no embedders
    // need to launch the single category settings UI directly.
    public static Intent createIntentForSiteSettingsSingleCategory(Context context,
            String profileName, boolean isIncognito, String categoryType, String categoryTitle) {
        return SettingsIntentHelper.createIntentForSiteSettingsSingleCategory(
                context, profileName, isIncognito, categoryType, categoryTitle);
    }
}
