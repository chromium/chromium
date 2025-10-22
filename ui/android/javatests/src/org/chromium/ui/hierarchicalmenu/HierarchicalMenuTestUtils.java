// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.hierarchicalmenu;

import android.view.View;

import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.List;

/** A utility class for testing hierarchical menus. */
public class HierarchicalMenuTestUtils {

    public static final int MENU_ITEM = 1;
    public static final int MENU_ITEM_WITH_SUBMENU = 2;
    public static final int MENU_ITEM_SUBMENU_HEADER = 3;

    public static final WritableObjectPropertyKey<CharSequence> TITLE =
            new WritableObjectPropertyKey<>();
    public static final WritableIntPropertyKey TITLE_ID = new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<View.OnClickListener> CLICK_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<View.@Nullable OnHoverListener> HOVER_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableObjectPropertyKey<View.OnKeyListener> KEY_LISTENER =
            new WritableObjectPropertyKey<>();
    public static final WritableBooleanPropertyKey ENABLED = new WritableBooleanPropertyKey();
    public static final WritableBooleanPropertyKey IS_HIGHLIGHTED =
            new WritableBooleanPropertyKey();
    public static final WritableIntPropertyKey MENU_ITEM_ID = new WritableIntPropertyKey();
    public static final WritableObjectPropertyKey<List<ListItem>> SUBMENU_ITEMS =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_MENU_ITEM_KEYS =
            new PropertyKey[] {TITLE, CLICK_LISTENER, ENABLED, IS_HIGHLIGHTED, MENU_ITEM_ID};

    public static final PropertyKey[] ALL_SUBMENU_ITEM_KEYS =
            new PropertyKey[] {
                TITLE,
                CLICK_LISTENER,
                ENABLED,
                IS_HIGHLIGHTED,
                MENU_ITEM_ID,
                SUBMENU_ITEMS,
                KEY_LISTENER
            };

    /**
     * @return An implementation of {@link HierarchicalMenuKeyProvider} using the keys defined in
     *     this class.
     */
    public static HierarchicalMenuKeyProvider createKeyProvider() {
        return new HierarchicalMenuKeyProvider() {
            @Override
            public WritableObjectPropertyKey<View.OnClickListener> getClickListenerKey() {
                return CLICK_LISTENER;
            }

            @Override
            public WritableBooleanPropertyKey getEnabledKey() {
                return ENABLED;
            }

            @Override
            public WritableObjectPropertyKey<View.OnHoverListener> getHoverListenerKey() {
                return HOVER_LISTENER;
            }

            @Override
            public WritableObjectPropertyKey<CharSequence> getTitleKey() {
                return TITLE;
            }

            @Override
            public WritableIntPropertyKey getTitleIdKey() {
                return TITLE_ID;
            }

            @Override
            public WritableObjectPropertyKey<View.OnKeyListener> getKeyListenerKey() {
                return KEY_LISTENER;
            }

            @Override
            public WritableObjectPropertyKey<List<ListItem>> getSubmenuItemsKey() {
                return SUBMENU_ITEMS;
            }

            @Override
            public WritableBooleanPropertyKey getIsHighlightedKey() {
                return IS_HIGHLIGHTED;
            }
        };
    }
}
