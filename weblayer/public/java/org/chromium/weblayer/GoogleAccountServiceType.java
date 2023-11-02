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
@IntDef({GoogleAccountServiceType.SIGNOUT, GoogleAccountServiceType.ADD_SESSION,
        GoogleAccountServiceType.DEFAULT})
@Retention(RetentionPolicy.SOURCE)
@interface GoogleAccountServiceType {
    /**
     * Logout all existing sessions.
     */
    int SIGNOUT = org.chromium.weblayer_private.interfaces.GoogleAccountServiceType.SIGNOUT;

    /**
     * Add or re-authenticate an account.
     */
    int ADD_SESSION = org.chromium.weblayer_private.interfaces.GoogleAccountServiceType.ADD_SESSION;

    /**
     * All other cases.
     */
    int DEFAULT = org.chromium.weblayer_private.interfaces.GoogleAccountServiceType.DEFAULT;
}
