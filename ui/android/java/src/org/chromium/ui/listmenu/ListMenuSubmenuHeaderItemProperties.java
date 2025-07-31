// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** The properties controlling submenu header item in context menus. */
@NullMarked
public class ListMenuSubmenuHeaderItemProperties {
    public static final PropertyModel.WritableObjectPropertyKey<View.OnKeyListener> KEY_LISTENER =
            new PropertyModel.WritableObjectPropertyKey<>();
    public static final PropertyKey[] ALL_KEYS = {TITLE, CLICK_LISTENER, KEY_LISTENER, ENABLED};
}
