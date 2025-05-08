// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** The properties controlling checkmark-type items in context menus. */
@NullMarked
public class ContextMenuCheckItemProperties {
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey MENU_ITEM_ID = new WritableIntPropertyKey();
    // The ON_CLICK should update the model (if needed).
    public static final WritableObjectPropertyKey<Runnable> ON_CLICK =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey ENABLED = new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey CHECKED = new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {TITLE, MENU_ITEM_ID, ON_CLICK, ENABLED, CHECKED};
}
