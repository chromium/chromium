// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.HOVER_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.IS_HIGHLIGHTED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;
import static org.chromium.ui.listmenu.ListMenuRadioItemProperties.SELECTED;

import android.view.View;
import android.widget.RadioButton;
import android.widget.TextView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View binder for a context menu item with radio button (of type {@code
 * ListItemType.LIST_MENU_ITEM_WITH_RADIO_BUTTON}, with property keys {@link
 * ListMenuRadioItemProperties}).
 */
@NullMarked
class ListMenuItemWithRadioButtonViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        RadioButton radioButton = view.findViewById(R.id.radio_button);
        TextView title = view.findViewById(R.id.radio_button_title);
        if (propertyKey == TITLE) {
            title.setText(model.get(TITLE));
        } else if (propertyKey == ENABLED) {
            radioButton.setEnabled(model.get(ENABLED));
            title.setEnabled(model.get(ENABLED));
        } else if (propertyKey == SELECTED) {
            radioButton.setChecked(model.get(SELECTED));
        } else if (propertyKey == CLICK_LISTENER) {
            view.setOnClickListener(model.get(CLICK_LISTENER));
        } else if (propertyKey == HOVER_LISTENER) {
            view.setOnHoverListener(model.get(HOVER_LISTENER));
        } else if (propertyKey == IS_HIGHLIGHTED) {
            view.setHovered(model.get(IS_HIGHLIGHTED));
        }
    }
}
