// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.HOVER_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.IS_HIGHLIGHTED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.KEY_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE_ID;
import static org.chromium.ui.listmenu.ListMenuSubmenuItemProperties.SUBMENU_ITEMS;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;
import android.widget.ListView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.R;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController.SubmenuHeaderFactory;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuKeyProvider;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.Collection;
import java.util.List;
import java.util.Set;

@NullMarked
public class ListMenuUtils {
    /**
     * Creates and configures a {@link ModelListAdapter} for the context menu.
     *
     * <p>This adapter handles different {@link ListItemType}s for context menu items, dividers, and
     * headers, and provides custom logic for determining item enabled status and retrieving item
     * IDs.
     *
     * @param listItems The {@link ModelList} containing the items to be displayed in the menu.
     * @return A configured {@link ModelListAdapter} ready to be set on the {@link ListView}.
     */
    public static ModelListAdapter createAdapter(ModelList listItems) {
        return createAdapter(listItems, Set.of(), /* delegate= */ null);
    }

    /**
     * Creates and configures a {@link ModelListAdapter} for the context menu.
     *
     * <p>This adapter handles different {@link ListItemType}s for context menu items, dividers, and
     * headers, and provides custom logic for determining item enabled status and retrieving item
     * IDs.
     *
     * @param listItems The {@link ModelList} containing the items to be displayed in the menu.
     * @param disabledTypes Additional integer types which should not be enabled in the adapter.
     * @param delegate The {@link ListMenu.Delegate} used to handle menu clicks. If not provided,
     *     the item's CLICK_LISTENER or listMenu's onMenuItemSelected method will be used.
     * @return A configured {@link ModelListAdapter} ready to be set on the {@link ListView}.
     */
    public static ListMenuItemAdapter createAdapter(
            ModelList listItems,
            Collection<Integer> disabledTypes,
            ListMenu.@Nullable Delegate delegate) {
        ListMenuItemAdapter adapter = new ListMenuItemAdapter(listItems, disabledTypes, delegate);

        adapter.registerType(
                ListItemType.DIVIDER,
                new LayoutViewBuilder(R.layout.list_section_divider),
                (m, v, p) -> {});
        adapter.registerType(
                ListItemType.MENU_ITEM,
                new LayoutViewBuilder(R.layout.list_menu_item),
                ListMenuItemViewBinder::binder);
        adapter.registerType(
                ListItemType.MENU_ITEM_WITH_CHECKBOX,
                new LayoutViewBuilder<>(R.layout.list_menu_checkbox),
                ListMenuItemWithCheckboxViewBinder::bind);
        adapter.registerType(
                ListItemType.MENU_ITEM_WITH_RADIO_BUTTON,
                new LayoutViewBuilder<>(R.layout.list_menu_radio_button),
                ListMenuItemWithRadioButtonViewBinder::bind);
        adapter.registerType(
                ListItemType.MENU_ITEM_WITH_SUBMENU,
                new LayoutViewBuilder<>(R.layout.list_menu_submenu_parent_row),
                ListMenuItemWithSubmenuViewBinder::bind);
        adapter.registerType(
                ListItemType.SUBMENU_HEADER,
                new LayoutViewBuilder<>(R.layout.list_menu_submenu_header),
                ListMenuSubmenuHeaderViewBinder::bind);

        return adapter;
    }

    /** Returns whether {@param item} has a click listener. */
    public static boolean hasClickListener(ListItem item) {
        return item.model != null
                && item.model.containsKey(CLICK_LISTENER)
                && item.model.get(CLICK_LISTENER) != null;
    }

    /**
     * Constructs a {@link ModelList} containing the submenu items of a given parent item.
     *
     * @param item The parent {@link ListItem} that contains the submenu.
     * @return A new {@link ModelList} populated with the children of the given item.
     */
    public static ModelList getModelListSubtree(ListItem item) {
        ModelList modelList = new ModelList();
        for (ListItem listItem : item.model.get(SUBMENU_ITEMS)) {
            modelList.add(listItem);
        }
        return modelList;
    }

    /**
     * Calculates the Rect of a given View in the coordinate space of a root View. This is useful
     * when you need to position a UI element (like a popup) relative to a specific view, but need
     * the coordinates to be based on the root view's visible area, which accounts for when the
     * window is not placed in the origin of the screen.
     *
     * @param view The View whose position and size are to be calculated.
     * @return A new {@link Rect} containing the coordinates and dimensions of the {@code view}
     *     relative to the visible frame of the {@code rootView}.
     */
    public static Rect getViewRectRelativeToItsRootView(View view) {
        Rect rootViewRect = new Rect();
        view.getRootView().getWindowVisibleDisplayFrame(rootViewRect);
        int[] viewCoordinates = new int[2];
        view.getLocationOnScreen(viewCoordinates);

        int left = viewCoordinates[0] - rootViewRect.left;
        int top = viewCoordinates[1] - rootViewRect.top;

        return new Rect(left, top, left + view.getWidth(), top + view.getHeight());
    }

    /**
     * Creates an instance of {@link HierarchicalMenuController} for {@link ListMenu}.
     *
     * @param context The {@link Context} for the controller to use.
     */
    public static HierarchicalMenuController createHierarchicalMenuController(Context context) {
        HierarchicalMenuKeyProvider keyProvider = new ListMenuUtils.ListMenuKeyProvider();
        SubmenuHeaderFactory headerFactory =
                (clickedItem, backRunnable) -> {
                    PropertyModel.Builder builder =
                            new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS);
                    HierarchicalMenuController.populateDefaultHeaderProperties(
                            builder,
                            keyProvider,
                            clickedItem.model.get(ListMenuItemProperties.TITLE),
                            backRunnable);
                    return new ListItem(ListItemType.SUBMENU_HEADER, builder.build());
                };
        return new HierarchicalMenuController(context, keyProvider, headerFactory);
    }

    public static class ListMenuKeyProvider implements HierarchicalMenuKeyProvider {
        @Override
        public WritableObjectPropertyKey<View.@Nullable OnClickListener> getClickListenerKey() {
            return CLICK_LISTENER;
        }

        @Override
        public WritableBooleanPropertyKey getEnabledKey() {
            return ENABLED;
        }

        @Override
        public WritableObjectPropertyKey<View.@Nullable OnHoverListener> getHoverListenerKey() {
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
    }
}
