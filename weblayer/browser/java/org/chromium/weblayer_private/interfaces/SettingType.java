// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({SettingType.BASIC_SAFE_BROWSING_ENABLED, SettingType.UKM_ENABLED,
        SettingType.EXTENDED_REPORTING_SAFE_BROWSING_ENABLED,
        SettingType.REAL_TIME_SAFE_BROWSING_ENABLED, SettingType.NETWORK_PREDICTION_ENABLED})
@Retention(RetentionPolicy.SOURCE)
public @interface SettingType {
    int BASIC_SAFE_BROWSING_ENABLED = 0;
    int UKM_ENABLED = 1;
    int EXTENDED_REPORTING_SAFE_BROWSING_ENABLED = 2;
    int REAL_TIME_SAFE_BROWSING_ENABLED = 3;
    int NETWORK_PREDICTION_ENABLED = 4;
}
