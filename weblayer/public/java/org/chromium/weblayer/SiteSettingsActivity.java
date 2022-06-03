// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.content.Intent;

/**
 * An Activity that displays various Site Settings UIs.
 *
 * @deprecated Use SettingsActivity instead
 */
@Deprecated
public class SiteSettingsActivity extends SettingsActivity {
    private static boolean sActivityNotExportedChecked;

    /** @deprecated Use SettingsActivity#createIntentForSiteSettingsCategoryList instead */
    @Deprecated
    public static Intent createIntentForCategoryList(Context context, String profileName) {
        return SettingsActivity.createIntentForSiteSettingsCategoryList(context, profileName);
    }

    /**
     * @deprecated Use SettingsActivity#createIntentForSiteSettingsCategoryList instead
     */
    @Deprecated
    public static Intent createIntentForCategoryList(
            Context context, String profileName, boolean isIncognito) {
        return SettingsActivity.createIntentForSiteSettingsCategoryList(
                context, profileName, isIncognito);
    }

    @Override
    protected boolean markActivityExportChecked() {
        boolean oldValue = sActivityNotExportedChecked;
        sActivityNotExportedChecked = true;
        return oldValue;
    }
}
