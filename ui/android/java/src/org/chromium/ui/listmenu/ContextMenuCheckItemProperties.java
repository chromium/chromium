// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.MENU_ITEM_ID;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties controlling checkmark-type items in context menus. */
@NullMarked
public class ContextMenuCheckItemProperties {
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey ENABLED = new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey CHECKED = new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
        TITLE, MENU_ITEM_ID, CLICK_LISTENER, ENABLED, CHECKED
    };
}
