// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * @hide
 */
@IntDef({BrowsingDataType.COOKIES_AND_SITE_DATA, BrowsingDataType.CACHE,
        BrowsingDataType.SITE_SETTINGS})
@Retention(RetentionPolicy.SOURCE)
@interface BrowsingDataType {
    int COOKIES_AND_SITE_DATA =
            org.chromium.weblayer_private.interfaces.BrowsingDataType.COOKIES_AND_SITE_DATA;
    int CACHE = org.chromium.weblayer_private.interfaces.BrowsingDataType.CACHE;
    int SITE_SETTINGS = org.chromium.weblayer_private.interfaces.BrowsingDataType.SITE_SETTINGS;
}
