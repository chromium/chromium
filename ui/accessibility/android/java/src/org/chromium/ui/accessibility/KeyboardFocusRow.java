// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.accessibility;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

/** Keeps track of which browser UI pane or control row currently has keyboard focus. */
@NullMarked
@IntDef({
    KeyboardFocusRow.CONTENT_PANE,
    KeyboardFocusRow.TAB_STRIP,
    KeyboardFocusRow.OMNIBOX,
    KeyboardFocusRow.BOOKMARKS_BAR,
    KeyboardFocusRow.SIDE_PANEL,
    KeyboardFocusRow.VERTICAL_TABS,
    KeyboardFocusRow.MESSAGE
})
public @interface KeyboardFocusRow {
    /**
     * Focus targets the content pane (e.g. web contents or native page), or no tracked browser UI
     * row currently holds focus.
     */
    int CONTENT_PANE = 0;

    /** The focus is within the tab strip. */
    int TAB_STRIP = 1;

    /** The focus is on the Omnibox */
    int OMNIBOX = 2;

    /** The focus is on the bookmarks bar (tab groups, bookmarks...). */
    int BOOKMARKS_BAR = 3;

    /** The focus is on the Side Panel (e.g. GLiC, AIM, etc). */
    int SIDE_PANEL = 4;

    /** The focus is on the Vertical Tabs rail. */
    int VERTICAL_TABS = 5;

    /** The focus is on the Message UI. */
    int MESSAGE = 6;
}
