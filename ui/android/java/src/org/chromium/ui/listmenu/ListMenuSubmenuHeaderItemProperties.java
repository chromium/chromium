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

/** The properties controlling submenu header item in context menus. */
@NullMarked
public class ListMenuSubmenuHeaderItemProperties {
    public static final PropertyKey[] ALL_KEYS = {TITLE, CLICK_LISTENER, KEY_LISTENER, ENABLED};
}
