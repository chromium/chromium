// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * @hide
 */
@IntDef({SettingType.BASIC_SAFE_BROWSING_ENABLED, SettingType.UKM_ENABLED,
        SettingType.EXTENDED_REPORTING_SAFE_BROWSING_ENABLED,
        SettingType.REAL_TIME_SAFE_BROWSING_ENABLED})
@Retention(RetentionPolicy.SOURCE)
@interface SettingType {
    /**
     * Allows the embedder to set whether it wants to disable/enable the Safe Browsing functionality
     * (which checks that the loaded URLs are safe). Safe Browsing is enabled by default.
     */
    int BASIC_SAFE_BROWSING_ENABLED =
            org.chromium.weblayer_private.interfaces.SettingType.BASIC_SAFE_BROWSING_ENABLED;
    /**
     * Allows the embedder to enable URL-Keyed Metrics. Disabled by default.
     */
    int UKM_ENABLED = org.chromium.weblayer_private.interfaces.SettingType.UKM_ENABLED;

    /**
     * Allows the embedder to set whether it wants to enable/disable the Extended Reporting
     * functionality for Safe Browsing (SBER). This functionality helps improve security on the web
     * for everyone. It sends URLs of some pages you visit, limited system information, and some
     * page content to Google, to help discover new threats and protect everyone on the web.
     *
     * This setting is disabled by default, but can also be enabled by the user by checking a
     * checkbox in the Safe Browsing interstitial which is displayed when the user encounters a
     * dangerous web page. The setting persists on disk.
     *
     * Note: this setting applies when Safe Browsing is enabled (i.e. BASIC_SAFE_BROWSING_ENABLED
     * is true).
     */
    int EXTENDED_REPORTING_SAFE_BROWSING_ENABLED =
            org.chromium.weblayer_private.interfaces.SettingType
                    .EXTENDED_REPORTING_SAFE_BROWSING_ENABLED;

    /**
     * Allows the embedder to set whether it wants to enable/disable the Safe Browsing Real-time URL
     * checks. This functionality is disabled by default.
     *
     * Note: this setting applies when Safe Browsing is enabled (i.e. BASIC_SAFE_BROWSING_ENABLED
     * is true).
     */
    int REAL_TIME_SAFE_BROWSING_ENABLED =
            org.chromium.weblayer_private.interfaces.SettingType.REAL_TIME_SAFE_BROWSING_ENABLED;

    /**
     * Allows the embedder to enable/disable NoStatePrefetch. Enabled by default.
     */
    int NETWORK_PREDICTION_ENABLED =
            org.chromium.weblayer_private.interfaces.SettingType.NETWORK_PREDICTION_ENABLED;
}
