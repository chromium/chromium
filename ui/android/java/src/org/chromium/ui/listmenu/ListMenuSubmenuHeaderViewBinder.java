// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.HOVER_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID;
import static org.chromium.ui.listmenu.ListMenuItemProperties.IS_HIGHLIGHTED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.KEY_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TEXT_APPEARANCE_ID;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;

import android.content.res.Resources;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.annotation.StyleRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.widget.ImageViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View binder for a context menu submenu header (of type {@code
 * ListItemType.LIST_MENU_SUBMENU_HEADER}, with property keys {@link
 * ListMenuSubmenuHeaderItemProperties}).
 */
@NullMarked
class ListMenuSubmenuHeaderViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        TextView textView = view.findViewById(R.id.menu_row_text);
        if (propertyKey == TITLE) {
            textView.setText(model.get(TITLE));
        } else if (propertyKey == ENABLED) {
            textView.setEnabled(model.get(ENABLED));
        } else if (propertyKey == CLICK_LISTENER) {
            view.setOnClickListener(model.get(CLICK_LISTENER));
        } else if (propertyKey == KEY_LISTENER) {
            view.setOnKeyListener(model.get(KEY_LISTENER));
        } else if (propertyKey == HOVER_LISTENER) {
            view.setOnHoverListener(model.get(HOVER_LISTENER));
        } else if (propertyKey == IS_HIGHLIGHTED) {
            view.setHovered(model.get(IS_HIGHLIGHTED));
        } else if (propertyKey == TEXT_APPEARANCE_ID) {
            @StyleRes int textAppearanceId = model.get(TEXT_APPEARANCE_ID);
            if (textAppearanceId != Resources.ID_NULL) {
                textView.setTextAppearance(textAppearanceId);
            }
        } else if (propertyKey == ICON_TINT_COLOR_STATE_LIST_ID) {
            @ColorRes int iconTintColorId = model.get(ICON_TINT_COLOR_STATE_LIST_ID);
            ImageView icon = view.findViewById(R.id.menu_item_icon);
            if (icon == null) return;
            if (iconTintColorId != Resources.ID_NULL) {
                ImageViewCompat.setImageTintList(
                        icon,
                        AppCompatResources.getColorStateList(view.getContext(), iconTintColorId));
            } else {
                ImageViewCompat.setImageTintList(icon, null);
            }
        }
    }
}
