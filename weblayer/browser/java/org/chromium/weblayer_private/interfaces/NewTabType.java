// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({NewTabType.FOREGROUND_TAB, NewTabType.BACKGROUND_TAB, NewTabType.NEW_POPUP,
        NewTabType.NEW_WINDOW})
@Retention(RetentionPolicy.SOURCE)
public @interface NewTabType {
    int FOREGROUND_TAB = 0;
    int BACKGROUND_TAB = 1;
    int NEW_POPUP = 2;
    int NEW_WINDOW = 3;
}
