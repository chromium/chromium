// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.content.Intent;

import org.chromium.weblayer_private.interfaces.SiteSettingsIntentHelper;

/**
 * Helpers for writing tests for the Site Settings UI.
 */
public final class SiteSettingsTestUtils {
    // This can be removed if/when we move this into SiteSettingsActivity. Currently no embedders
    // need to launch the single site settings UI directly.
    public static Intent createIntentForSingleWebsite(
            Context context, String profileName, boolean isIncognito, String url) {
        return SiteSettingsIntentHelper.createIntentForSingleWebsite(
                context, profileName, isIncognito, url);
    }
}
