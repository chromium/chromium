// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

/** Keeps track of options for which row of the top controls currently has keyboard focus. */
@NullMarked
@IntDef({
    KeyboardFocusRow.NONE,
    KeyboardFocusRow.TAB_STRIP,
    KeyboardFocusRow.TOOLBAR,
    KeyboardFocusRow.BOOKMARKS_BAR
})
public @interface KeyboardFocusRow {
    /** The focus is not in one of the rows of top controls. */
    int NONE = 0;

    /** The focus is within the tab strip. */
    int TAB_STRIP = 1;

    /** The focus is on the toolbar (home, back, forward, refresh, omnibox...). */
    int TOOLBAR = 2;

    /** The focus is on the bookmarks bar (tab groups, bookmarks...). */
    int BOOKMARKS_BAR = 3;
}
