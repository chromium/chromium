// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import android.graphics.Bitmap;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.contextmenu.ContextMenuCoordinator.ListItemType;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * A converter from (<a
 * href="https://source.chromium.org/chromium/chromium/src/+/main:ui/base/models/menu_model.h">C++
 * MenuModel</a>) to a Java list of {@link ListItem} to be used in context menus.
 */
@NullMarked
@JNINamespace("ui")
public class MenuModelBridge {

    private final List<ListItem> mItems = new ArrayList<>();

    @CalledByNative
    private static MenuModelBridge create() {
        return new MenuModelBridge();
    }

    /** {@return A {@link MenuModelBridge} instance.} */
    private MenuModelBridge() {}

    /** {@return The list of {@link ListItem} held by this {@link MenuModelBridge}.} */
    public List<ListItem> getListItems() {
        return mItems;
    }

    /**
     * Adds a context menu item which triggers a command when activated.
     *
     * @param label The label to display.
     * @param bitmap The icon to display (or null if there should be no icon).
     * @param isEnabled Whether the command is enabled.
     * @param callback The callback to run when the command is activated.
     */
    @CalledByNative
    private void addCommand(
            @JniType("std::u16string") final String label,
            @JniType("SkBitmap") final @Nullable Bitmap bitmap,
            final boolean isEnabled,
            final Runnable callback) {
        PropertyModel.Builder modelBuilder =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TITLE, label)
                        .with(ListMenuItemProperties.START_ICON_BITMAP, bitmap)
                        .with(ListMenuItemProperties.ENABLED, isEnabled)
                        .with(ListMenuItemProperties.CLICK_LISTENER, (view) -> callback.run());
        mItems.add(new ListItem(ListItemType.CONTEXT_MENU_ITEM, modelBuilder.build()));
    }

    /**
     * Adds a context menu item with a checkbox.
     *
     * @param label The label to display.
     * @param isChecked Whether the checkbox is checked.
     * @param isEnabled Whether the checkbox and label are enabled.
     * @param callback The callback to run when the checkbox is clicked.
     */
    @CalledByNative
    private void addCheck(
            @JniType("std::u16string") final String label,
            final boolean isChecked,
            final boolean isEnabled,
            final Runnable callback) {
        PropertyModel.Builder modelBuilder =
                new PropertyModel.Builder(ContextMenuCheckItemProperties.ALL_KEYS)
                        .with(ContextMenuCheckItemProperties.TITLE, label)
                        .with(ContextMenuCheckItemProperties.CHECKED, isChecked)
                        .with(ContextMenuCheckItemProperties.ENABLED, isEnabled)
                        .with(ContextMenuCheckItemProperties.ON_CLICK, callback);
        mItems.add(
                new ListItem(ListItemType.CONTEXT_MENU_ITEM_WITH_CHECKBOX, modelBuilder.build()));
    }

    /**
     * Adds a context menu item with a radio button.
     *
     * @param label The label to display.
     * @param isSelected Whether the radio option is selected.
     * @param isEnabled Whether the radio option and label are enabled.
     * @param callback The callback to run when the radio option is selected.
     */
    @CalledByNative
    private void addRadioButton(
            @JniType("std::u16string") final String label,
            final boolean isSelected,
            final boolean isEnabled,
            final Runnable callback) {
        PropertyModel.Builder modelBuilder =
                new PropertyModel.Builder(ContextMenuRadioItemProperties.ALL_KEYS)
                        .with(ContextMenuRadioItemProperties.TITLE, label)
                        .with(ContextMenuRadioItemProperties.SELECTED, isSelected)
                        .with(ContextMenuRadioItemProperties.ENABLED, isEnabled)
                        .with(ContextMenuRadioItemProperties.ON_CLICK, callback);
        mItems.add(
                new ListItem(
                        ListItemType.CONTEXT_MENU_ITEM_WITH_RADIO_BUTTON, modelBuilder.build()));
    }

    /** Adds a divider to the context menu. */
    @CalledByNative
    private void addDivider() {
        // TODO(crbug.com/416222384): Update context menus to use incognito theming.
        mItems.add(new ListItem(ListItemType.DIVIDER, new PropertyModel()));
    }
}
