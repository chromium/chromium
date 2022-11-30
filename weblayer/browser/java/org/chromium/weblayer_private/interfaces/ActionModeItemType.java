// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({ActionModeItemType.SHARE, ActionModeItemType.WEB_SEARCH})
@Retention(RetentionPolicy.SOURCE)
public @interface ActionModeItemType {
    int SHARE = 1 << 0;
    int WEB_SEARCH = 1 << 1;
}
