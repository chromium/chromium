// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.chromium.ui.listmenu.ListMenuCheckItemProperties.CHECKED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.HOVER_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.IS_HIGHLIGHTED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;

import android.view.View;
import android.widget.CheckBox;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View binder for a context menu item with checkbox (of type {@code
 * ListItemType.LIST_MENU_ITEM_WITH_CHECKBOX}, with property keys {@link
 * ListMenuCheckItemProperties}).
 */
@NullMarked
class ListMenuItemWithCheckboxViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        CheckBox checkBox = view.findViewById(R.id.checkbox);
        TextView title = view.findViewById(R.id.checkbox_title);

        if (propertyKey == TITLE) {
            title.setText(model.get(TITLE));
        } else if (propertyKey == ENABLED) {
            checkBox.setEnabled(model.get(ENABLED));
            title.setEnabled(model.get(ENABLED));
        } else if (propertyKey == CHECKED) {
            checkBox.setChecked(model.get(CHECKED));
        } else if (propertyKey == CLICK_LISTENER) {
            view.setOnClickListener(model.get(CLICK_LISTENER));
        } else if (propertyKey == HOVER_LISTENER) {
            view.setOnHoverListener(model.get(HOVER_LISTENER));
        } else if (propertyKey == IS_HIGHLIGHTED) {
            view.setHovered(model.get(IS_HIGHLIGHTED));
        }
    }
}
