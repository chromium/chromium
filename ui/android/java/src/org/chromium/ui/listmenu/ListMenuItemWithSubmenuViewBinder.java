// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static android.view.View.GONE;
import static android.view.View.VISIBLE;

import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.START_ICON_BITMAP;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;
import static org.chromium.ui.listmenu.ListMenuSubmenuItemProperties.ON_HOVER;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.R;
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
        } else if (propertyKey == ON_HOVER) {
            // TODO(crbug.com/424580483): Implement flyout submenus.
        } else if (propertyKey == CLICK_LISTENER) {
            view.setOnClickListener(model.get(CLICK_LISTENER));
        }
    }
}
