// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.START_ICON_BITMAP;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;

import android.graphics.Bitmap;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
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
    private long mNativePtr;

    @CalledByNative
    private static MenuModelBridge create(long nativePtr) {
        return new MenuModelBridge(nativePtr);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    /** {@return A {@link MenuModelBridge} instance} */
    public MenuModelBridge(long nativePtr) {
        mNativePtr = nativePtr;
    }

    /** {@return The list of {@link ListItem} held by this {@link MenuModelBridge}} */
    public List<ListItem> getListItems() {
        return mItems;
    }

    /**
     * Returns the list of {@link ListItem} held by this {@link MenuModelBridge}, as a {@link
     * ModelList}.
     */
    public ModelList populateModelList() {
        ModelList result = new ModelList();
        // addAll asserts that the collection is nonempty, so we MUST perform this check.
        if (!mItems.isEmpty()) result.addAll(mItems);
        return result;
    }

    /**
     * Adds a context menu item which triggers a command when activated.
     *
     * @param label The label to display.
     * @param bitmap The icon to display (or null if there should be no icon).
     * @param isEnabled Whether the command is enabled.
     * @param indexForModelActivation The index for {@link Natives#activatedAt(long, int)}.
     */
    @CalledByNative
    private void addCommand(
            @JniType("std::u16string") final String label,
            @JniType("std::optional<SkBitmap>") final @Nullable Bitmap bitmap,
            final boolean isEnabled,
            final int indexForModelActivation) {
        PropertyModel.Builder modelBuilder =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(TITLE, label)
                        .with(START_ICON_BITMAP, bitmap)
                        .with(ENABLED, isEnabled)
                        .with(
                                CLICK_LISTENER,
                                (view) -> {
                                    if (mNativePtr == 0) return;
                                    RecordUserAction.record("ContextMenu.ExtensionItemClicked");
                                    MenuModelBridgeJni.get()
                                            .activatedAt(mNativePtr, indexForModelActivation);
                                });
        mItems.add(new ListItem(ListItemType.MENU_ITEM, modelBuilder.build()));
    }

    /**
     * Adds a context menu item with a checkbox.
     *
     * @param label The label to display.
     * @param isChecked Whether the checkbox is checked.
     * @param isEnabled Whether the checkbox and label are enabled.
     * @param indexForModelActivation The index for {@link Natives#activatedAt(long, int)}.
     */
    @CalledByNative
    private void addCheck(
            @JniType("std::u16string") final String label,
            final boolean isChecked,
            final boolean isEnabled,
            final int indexForModelActivation) {
        PropertyModel.Builder modelBuilder =
                new PropertyModel.Builder(ListMenuCheckItemProperties.ALL_KEYS)
                        .with(TITLE, label)
                        .with(ListMenuCheckItemProperties.CHECKED, isChecked)
                        .with(ENABLED, isEnabled)
                        .with(
                                CLICK_LISTENER,
                                (view) -> {
                                    if (mNativePtr == 0) return;
                                    RecordUserAction.record("ContextMenu.ExtensionItemClicked");
                                    MenuModelBridgeJni.get()
                                            .activatedAt(mNativePtr, indexForModelActivation);
                                });
        mItems.add(new ListItem(ListItemType.MENU_ITEM_WITH_CHECKBOX, modelBuilder.build()));
    }

    /**
     * Adds a context menu item with a radio button.
     *
     * @param label The label to display.
     * @param isSelected Whether the radio option is selected.
     * @param isEnabled Whether the radio option and label are enabled.
     * @param indexForModelActivation The index for {@link Natives#activatedAt(long, int)}.
     */
    @CalledByNative
    private void addRadioButton(
            @JniType("std::u16string") final String label,
            final boolean isSelected,
            final boolean isEnabled,
            final int indexForModelActivation) {
        PropertyModel.Builder modelBuilder =
                new PropertyModel.Builder(ListMenuRadioItemProperties.ALL_KEYS)
                        .with(TITLE, label)
                        .with(ListMenuRadioItemProperties.SELECTED, isSelected)
                        .with(ENABLED, isEnabled)
                        .with(
                                CLICK_LISTENER,
                                (view) -> {
                                    if (mNativePtr == 0) return;
                                    RecordUserAction.record("ContextMenu.ExtensionItemClicked");
                                    MenuModelBridgeJni.get()
                                            .activatedAt(mNativePtr, indexForModelActivation);
                                });
        mItems.add(new ListItem(ListItemType.MENU_ITEM_WITH_RADIO_BUTTON, modelBuilder.build()));
    }

    /**
     * Adds a context menu item with a radio button.
     *
     * @param label The label to display.
     * @param bitmap The icon to display (or null if there should be no icon).
     * @param isEnabled Whether the radio option and label are enabled.
     * @param submenuItems The items that will be under this submenu.
     */
    @CalledByNative
    private void addSubmenu(
            @JniType("std::u16string") final String label,
            @JniType("std::optional<SkBitmap>") final @Nullable Bitmap bitmap,
            final boolean isEnabled,
            MenuModelBridge submenuItems) {
        PropertyModel.Builder modelBuilder =
                new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS)
                        .with(TITLE, label)
                        .with(START_ICON_BITMAP, bitmap)
                        .with(ENABLED, isEnabled)
                        .with(ListMenuSubmenuItemProperties.SUBMENU_ITEMS, submenuItems.mItems);
        mItems.add(new ListItem(ListItemType.MENU_ITEM_WITH_SUBMENU, modelBuilder.build()));
    }

    /** Adds a divider to the context menu. */
    @CalledByNative
    private void addDivider() {
        // TODO(crbug.com/416222384): Update context menus to use incognito theming.
        mItems.add(new ListItem(ListItemType.DIVIDER, new PropertyModel()));
    }

    @CalledByNative
    private void destroyNative() {
        mNativePtr = 0;
    }

    @NativeMethods
    interface Natives {
        void activatedAt(long nativeMenuModelBridge, int i);
    }
}
