// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties controlling radio-button-type items in context menus. */
@NullMarked
public class ContextMenuRadioItemProperties {
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey MENU_ITEM_ID = new WritableIntPropertyKey();
    // The ON_CLICK should handle deselection of other radio button items & other model updates.
    public static final WritableObjectPropertyKey<Runnable> ON_CLICK =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey ENABLED = new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey SELECTED = new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {TITLE, MENU_ITEM_ID, ON_CLICK, ENABLED, SELECTED};
}
