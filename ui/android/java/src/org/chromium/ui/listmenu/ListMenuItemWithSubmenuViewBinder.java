// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static android.view.View.GONE;
import static android.view.View.VISIBLE;

import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CONTENT_DESCRIPTION;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.HOVER_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID;
import static org.chromium.ui.listmenu.ListMenuItemProperties.IS_HIGHLIGHTED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.KEY_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.START_ICON_BITMAP;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TEXT_APPEARANCE_ID;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TOOLTIP;
import static org.chromium.ui.listmenu.ListMenuSubmenuItemProperties.IS_EXPANDED;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.annotation.StyleRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.widget.ImageViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.R;
import org.chromium.ui.hierarchicalmenu.MenuItemWithSubmenuView;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View binder for a context menu item with submenu (of type {@code
 * ListItemType.CONTEXT_MENU_ITEM_WITH_SUBMENU}, with property keys {@link
 * ListMenuSubmenuItemProperties}).
 */
@NullMarked
class ListMenuItemWithSubmenuViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        TextView textView = view.findViewById(R.id.menu_row_text);
        if (propertyKey == TITLE) {
            textView.setText(model.get(TITLE));
        } else if (propertyKey == CONTENT_DESCRIPTION) {
            view.setContentDescription(model.get(CONTENT_DESCRIPTION));
        } else if (propertyKey == TOOLTIP) {
            view.setTooltipText(model.get(TOOLTIP));
        } else if (propertyKey == START_ICON_BITMAP) {
            ImageView icon = view.findViewById(org.chromium.ui.R.id.menu_item_icon);
            Bitmap bitmap = model.get(ListMenuItemProperties.START_ICON_BITMAP);
            if (bitmap == null) {
                icon.setVisibility(GONE);
            } else {
                Drawable drawable = new BitmapDrawable(view.getResources(), bitmap);
                icon.setImageDrawable(drawable);
                icon.setVisibility(VISIBLE);
            }
        } else if (propertyKey == ENABLED) {
            textView.setEnabled(model.get(ENABLED));
        } else if (propertyKey == CLICK_LISTENER) {
            view.setOnClickListener(model.get(CLICK_LISTENER));
        } else if (propertyKey == HOVER_LISTENER) {
            view.setOnHoverListener(model.get(HOVER_LISTENER));
        } else if (propertyKey == IS_HIGHLIGHTED) {
            view.setHovered(model.get(IS_HIGHLIGHTED));
        } else if (propertyKey == IS_EXPANDED) {
            ((MenuItemWithSubmenuView) view).setIsExpanded(model.get(IS_EXPANDED));
        } else if (propertyKey == ListMenuItemProperties.IS_TEXT_ELLIPSIZED_AT_END) {
            if (model.get(ListMenuItemProperties.IS_TEXT_ELLIPSIZED_AT_END)) {
                textView.setMaxLines(1);
                textView.setEllipsize(TextUtils.TruncateAt.END);
            } else {
                textView.setEllipsize(null);
                textView.setMaxLines(Integer.MAX_VALUE);
            }
        } else if (propertyKey == KEY_LISTENER) {
            view.setOnKeyListener(model.get(KEY_LISTENER));
        } else if (propertyKey == TEXT_APPEARANCE_ID) {
            @StyleRes int textAppearanceId = model.get(TEXT_APPEARANCE_ID);
            if (textAppearanceId != Resources.ID_NULL) {
                textView.setTextAppearance(textAppearanceId);
            }
        } else if (propertyKey == ICON_TINT_COLOR_STATE_LIST_ID) {
            @ColorRes int iconTintColorId = model.get(ICON_TINT_COLOR_STATE_LIST_ID);
            ImageView icon = view.findViewById(org.chromium.ui.R.id.menu_item_icon);
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
