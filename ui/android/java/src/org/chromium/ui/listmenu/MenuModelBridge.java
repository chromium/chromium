// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.chromium.ui.listmenu.BasicListMenu.ListMenuItemType.MENU_ITEM;
import static org.chromium.ui.listmenu.BasicListMenu.buildMenuDivider;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.START_ICON_BITMAP;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;

import android.graphics.Bitmap;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
                        .with(TITLE, label)
                        .with(START_ICON_BITMAP, bitmap)
                        .with(ENABLED, isEnabled)
                        .with(CLICK_LISTENER, (view) -> callback.run());
        mItems.add(new ListItem(MENU_ITEM, modelBuilder.build()));
    }

    /** Adds a divider to the context menu. */
    @CalledByNative
    private void addDivider() {
        // TODO(crbug.com/416222384): Update context menus to use incognito theming.
        mItems.add(buildMenuDivider(/* isIncognito= */ false));
    }
}
