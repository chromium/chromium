// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.KEY_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/** The properties controlling checkmark-type items in context menus. */
@NullMarked
public class ListMenuCheckItemProperties {
    public static final WritableBooleanPropertyKey CHECKED = new WritableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
        TITLE, CLICK_LISTENER, ENABLED, CHECKED, KEY_LISTENER
    };
}
