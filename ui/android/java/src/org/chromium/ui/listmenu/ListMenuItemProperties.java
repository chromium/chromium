// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * The properties controlling the state of the list menu items. Any given list item can have either
 * one start icon or one end icon but not both.
 */
public class ListMenuItemProperties {
    // TODO(crbug.com/40738791): Consider passing menu item title through TITLE property instead of
    // TITLE_ID.
    public static final WritableIntPropertyKey TITLE_ID = new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<String> TITLE = new WritableObjectPropertyKey<>();
    // A11y content description of menu item
    public static final WritableObjectPropertyKey<String> CONTENT_DESCRIPTION =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey START_ICON_ID = new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<Drawable> START_ICON_DRAWABLE =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey END_ICON_ID = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey GROUP_ID = new WritableIntPropertyKey();
    public static final WritableIntPropertyKey MENU_ITEM_ID = new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<View.OnClickListener> CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<Intent> INTENT =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey ENABLED = new WritableBooleanPropertyKey();
    public static final WritableIntPropertyKey ICON_TINT_COLOR_STATE_LIST_ID =
            new WritableIntPropertyKey();
    public static final ReadableIntPropertyKey TEXT_APPEARANCE_ID = new ReadableIntPropertyKey();
    public static final ReadableBooleanPropertyKey IS_TEXT_ELLIPSIZED_AT_END =
            new ReadableBooleanPropertyKey();
    public static final ReadableBooleanPropertyKey KEEP_START_ICON_SPACING_WHEN_HIDDEN =
            new ReadableBooleanPropertyKey();

    public static final PropertyKey[] ALL_KEYS = {
        TITLE_ID,
        TITLE,
        CONTENT_DESCRIPTION,
        START_ICON_ID,
        START_ICON_DRAWABLE,
        END_ICON_ID,
        GROUP_ID,
        MENU_ITEM_ID,
        CLICK_LISTENER,
        INTENT,
        ENABLED,
        ICON_TINT_COLOR_STATE_LIST_ID,
        TEXT_APPEARANCE_ID,
        IS_TEXT_ELLIPSIZED_AT_END,
        KEEP_START_ICON_SPACING_WHEN_HIDDEN
    };
}
