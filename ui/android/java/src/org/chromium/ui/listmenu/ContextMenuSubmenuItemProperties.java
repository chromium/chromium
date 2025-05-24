// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.List;

/** The properties controlling submenu-type items in context menus. */
@NullMarked
public class ContextMenuSubmenuItemProperties {
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey MENU_ITEM_ID = new WritableIntPropertyKey();
    // The ON_CLICK should enter the submenu (for drilldown).
    public static final WritableObjectPropertyKey<Runnable> ON_CLICK =
            new WritableObjectPropertyKey<>();
    // The ON_HOVER should show the flyout on mouse hover or keyboard focus.
    public static final WritableObjectPropertyKey<Runnable> ON_HOVER =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey ENABLED = new WritableBooleanPropertyKey();
    // A list of PropertyModels, each of which represents a context menu item. Some of these menu
    // items may represent context menus themselves.
    public static final WritableObjectPropertyKey<List<PropertyModel>> SUBMENU_ITEMS =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {
        TITLE, MENU_ITEM_ID, ON_CLICK, ON_HOVER, ENABLED, SUBMENU_ITEMS
    };
}
