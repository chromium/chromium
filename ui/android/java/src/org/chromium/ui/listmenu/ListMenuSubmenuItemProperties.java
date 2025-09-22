// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CONTENT_DESCRIPTION;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.HOVER_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.IS_HIGHLIGHTED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.IS_TEXT_ELLIPSIZED_AT_END;
import static org.chromium.ui.listmenu.ListMenuItemProperties.KEY_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.START_ICON_BITMAP;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.List;

/** The properties controlling submenu-type items in context menus. */
@NullMarked
public class ListMenuSubmenuItemProperties {
    public static final WritableObjectPropertyKey<List<ListItem>> SUBMENU_ITEMS =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {
        TITLE,
        CONTENT_DESCRIPTION,
        START_ICON_BITMAP,
        CLICK_LISTENER,
        HOVER_LISTENER,
        IS_HIGHLIGHTED,
        ENABLED,
        SUBMENU_ITEMS,
        IS_TEXT_ELLIPSIZED_AT_END,
        KEY_LISTENER
    };
}
