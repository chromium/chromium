// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({ScrollNotificationType.DIRECTION_CHANGED_UP,
        ScrollNotificationType.DIRECTION_CHANGED_DOWN})
@Retention(RetentionPolicy.SOURCE)
public @interface ScrollNotificationType {
    int DIRECTION_CHANGED_UP = 0;
    int DIRECTION_CHANGED_DOWN = 1;
}
