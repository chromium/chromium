// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

// LINT.IfChange

/** The type of item in a list menu. */
@NullMarked
@Retention(RetentionPolicy.SOURCE)
@IntDef({
    ListItemType.DIVIDER,
    ListItemType.MENU_ITEM,
    ListItemType.MENU_ITEM_WITH_CHECKBOX,
    ListItemType.MENU_ITEM_WITH_RADIO_BUTTON,
    ListItemType.MENU_ITEM_WITH_SUBMENU,
    ListItemType.SUBMENU_HEADER,
})
public @interface ListItemType {
    int DIVIDER = 0;
    int MENU_ITEM = 1;
    int MENU_ITEM_WITH_CHECKBOX = 2;
    int MENU_ITEM_WITH_RADIO_BUTTON = 3;
    int MENU_ITEM_WITH_SUBMENU = 4;
    int SUBMENU_HEADER = 5;
}

// LINT.ThenChange(//chrome/android/java/src/org/chromium/chrome/browser/contextmenu/ContextMenuCoordinator.java)
