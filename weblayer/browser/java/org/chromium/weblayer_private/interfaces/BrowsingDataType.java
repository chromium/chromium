// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({BrowsingDataType.COOKIES_AND_SITE_DATA, BrowsingDataType.CACHE})
@Retention(RetentionPolicy.SOURCE)
public @interface BrowsingDataType {
    int COOKIES_AND_SITE_DATA = 0;
    int CACHE = 1;
}
