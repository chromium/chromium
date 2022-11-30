// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({GoogleAccountServiceType.SIGNOUT, GoogleAccountServiceType.ADD_SESSION,
        GoogleAccountServiceType.DEFAULT})
@Retention(RetentionPolicy.SOURCE)
public @interface GoogleAccountServiceType {
    int SIGNOUT = 0;
    int ADD_SESSION = 1;
    int DEFAULT = 2;
}
