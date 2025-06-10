// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * The type of item in a context menu. This is used for context menus for image/link/video etc and
 * empty space (see ContextMenuCoordinator.java and ChromeContextMenuPopulator.java), as well as
 * selected text (see SelectionDropdownMenuDelegate.java).
 */
@NullMarked
@Retention(RetentionPolicy.SOURCE)
@IntDef({
    ListItemType.DIVIDER,
    ListItemType.HEADER,
    ListItemType.CONTEXT_MENU_ITEM,
    ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON,
    ListItemType.CONTEXT_MENU_ITEM_WITH_CHECKBOX,
    ListItemType.CONTEXT_MENU_ITEM_WITH_RADIO_BUTTON,
})
public @interface ListItemType {
    int DIVIDER = 0;
    int HEADER = 1;
    int CONTEXT_MENU_ITEM = 2;
    int CONTEXT_MENU_ITEM_WITH_ICON_BUTTON = 3;
    int CONTEXT_MENU_ITEM_WITH_CHECKBOX = 4;
    int CONTEXT_MENU_ITEM_WITH_RADIO_BUTTON = 5;
}
