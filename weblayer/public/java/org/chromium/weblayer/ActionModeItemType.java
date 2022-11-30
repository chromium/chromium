// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@IntDef({ActionModeItemType.SHARE, ActionModeItemType.WEB_SEARCH})
@Retention(RetentionPolicy.SOURCE)
@interface ActionModeItemType {
    int SHARE = org.chromium.weblayer_private.interfaces.ActionModeItemType.SHARE;
    int WEB_SEARCH = org.chromium.weblayer_private.interfaces.ActionModeItemType.WEB_SEARCH;
}
