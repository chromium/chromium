// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.annotation.IntDef;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * @hide
 */
@IntDef({NewTabType.FOREGROUND_TAB, NewTabType.BACKGROUND_TAB, NewTabType.NEW_POPUP,
        NewTabType.NEW_WINDOW})
@Retention(RetentionPolicy.SOURCE)
public @interface NewTabType {
    /**
     * The page requested a new tab to be shown active.
     */
    int FOREGROUND_TAB = org.chromium.weblayer_private.interfaces.NewTabType.FOREGROUND_TAB;

    /**
     * The page requested a new tab in the background. Generally, this is only encountered when
     * keyboard modifiers are used.
     */
    int BACKGROUND_TAB = org.chromium.weblayer_private.interfaces.NewTabType.BACKGROUND_TAB;
    /**
     * The page requested the tab to open a new popup. A popup generally shows minimal ui
     * affordances, such as no tabstrip. On a phone, this is generally the same as
     * NEW_TAB_MODE_FOREGROUND_TAB.
     */
    int NEW_POPUP = org.chromium.weblayer_private.interfaces.NewTabType.NEW_POPUP;

    /**
     * The page requested the tab to open in a new window. On a phone, this is generally the
     * same as NEW_TAB_MODE_FOREGROUND_TAB.
     */
    int NEW_WINDOW = org.chromium.weblayer_private.interfaces.NewTabType.NEW_WINDOW;
}
