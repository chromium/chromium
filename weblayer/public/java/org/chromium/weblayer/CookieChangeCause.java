// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * @hide
 */
@IntDef({CookieChangeCause.INSERTED, CookieChangeCause.EXPLICIT, CookieChangeCause.UNKNOWN_DELETION,
        CookieChangeCause.OVERWRITE, CookieChangeCause.EXPIRED, CookieChangeCause.EVICTED,
        CookieChangeCause.EXPIRED_OVERWRITE})
@Retention(RetentionPolicy.SOURCE)
public @interface CookieChangeCause {
    /** The cookie was inserted. */
    int INSERTED = org.chromium.weblayer_private.interfaces.CookieChangeCause.INSERTED;
    /** The cookie was changed directly by a consumer's action. */
    int EXPLICIT = org.chromium.weblayer_private.interfaces.CookieChangeCause.EXPLICIT;
    /** The cookie was deleted, but no more details are known. */
    int UNKNOWN_DELETION =
            org.chromium.weblayer_private.interfaces.CookieChangeCause.UNKNOWN_DELETION;
    /** The cookie was automatically removed due to an insert operation that overwrote it. */
    int OVERWRITE = org.chromium.weblayer_private.interfaces.CookieChangeCause.OVERWRITE;
    /** The cookie was automatically removed as it expired. */
    int EXPIRED = org.chromium.weblayer_private.interfaces.CookieChangeCause.EXPIRED;
    /** The cookie was automatically evicted during garbage collection. */
    int EVICTED = org.chromium.weblayer_private.interfaces.CookieChangeCause.EVICTED;
    /** The cookie was overwritten with an already-expired expiration date. */
    int EXPIRED_OVERWRITE =
            org.chromium.weblayer_private.interfaces.CookieChangeCause.EXPIRED_OVERWRITE;
}
