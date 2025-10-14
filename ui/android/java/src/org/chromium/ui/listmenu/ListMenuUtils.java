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
import android.content.res.Resources;
import android.view.View;
import android.widget.ListView;

import androidx.annotation.StringRes;
import androidx.core.view.ViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.R;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuKeyProvider;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.ListObservable.ListObserver;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
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

    /** Watches a ModelList and updates the accessibility pane title of the View accordingly. */
    public static class AccessibilityListObserver implements ListObserver<Void> {

        private final View mView;
        private final @Nullable ListView mHeaderView;
        private final ListView mContentView;
        private final Context mContext;
        private final @Nullable ModelList mHeaderModelList;
        private final ModelList mContentModelList;

        /**
         * Returns a {@link AccessibilityListObserver} that reacts to changes in {@param
         * headerModelList and {@param contentModelList}, the are backing models for {@param view}.
         */
        public AccessibilityListObserver(
                View parentView,
                @Nullable ListView headerView,
                ListView contentView,
                @Nullable ModelList headerModelList,
                ModelList contentModelList) {
            mView = parentView;
            mHeaderView = headerView;
            mContentView = contentView;
            mContext = parentView.getContext();
            mHeaderModelList = headerModelList;
            mContentModelList = contentModelList;
        }

        // Note: because ListMenuUtils methods use ModelList#set, they trigger onItemRangeChanged.
        @Override
        public void onItemRangeChanged(
                ListObservable<Void> source, int index, int count, @Nullable Void payload) {
            if (index != 0) return; // If the 1st element wasn't changed, the "header" is the same.
            String accessibilityPaneTitle =
                    mContext.getString(R.string.listmenu_a11y_default_pane_title);
            Object firstItem = null;
            if (mHeaderModelList != null && !mHeaderModelList.isEmpty()) {
                firstItem = mHeaderModelList.get(0);
            } else if (!mContentModelList.isEmpty()) {
                firstItem = mContentModelList.get(0);
            }
            if (firstItem instanceof ListItem firstListItem && firstListItem.model != null) {
                if (firstListItem.model.containsKey(TITLE)
                        && firstListItem.model.get(TITLE) != null) {
                    CharSequence title = firstListItem.model.get(TITLE);
                    if (title.length() != 0) {
                        accessibilityPaneTitle = String.valueOf(title);
                    }
                } else if (firstListItem.model.containsKey(TITLE_ID)) {
                    @StringRes int titleId = firstListItem.model.get(TITLE_ID);
                    if (titleId != Resources.ID_NULL) {
                        accessibilityPaneTitle =
                                mContext.getString(firstListItem.model.get(TITLE_ID));
                    }
                }
            }
            ViewCompat.setAccessibilityPaneTitle(mView, accessibilityPaneTitle);
            // The method calls below ensure that when we transition to a different submenu, the
            // keyboard focus goes to the topmost element.
            mContentView.setSelection(0);
            if (mHeaderView != null && mHeaderModelList != null && !mHeaderModelList.isEmpty())
                mHeaderView.setSelection(0);
            mView.requestFocus();
        }
    }

    public static class ListMenuKeyProvider implements HierarchicalMenuKeyProvider {
        @Override
        public PropertyKey[] getAllHeaderItemKeys() {
            return ListMenuSubmenuItemProperties.ALL_KEYS;
        }

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

        @Override
        public int getSubmenuHeaderType() {
            return ListItemType.SUBMENU_HEADER;
        }
    }
}
