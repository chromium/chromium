// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({CookieChangeCause.INSERTED, CookieChangeCause.EXPLICIT, CookieChangeCause.UNKNOWN_DELETION,
        CookieChangeCause.OVERWRITE, CookieChangeCause.EXPIRED, CookieChangeCause.EVICTED,
        CookieChangeCause.EXPIRED_OVERWRITE})
@Retention(RetentionPolicy.SOURCE)
public @interface CookieChangeCause {
    int INSERTED = 0;
    int EXPLICIT = 1;
    int UNKNOWN_DELETION = 2;
    int OVERWRITE = 3;
    int EXPIRED = 4;
    int EVICTED = 5;
    int EXPIRED_OVERWRITE = 6;
}
