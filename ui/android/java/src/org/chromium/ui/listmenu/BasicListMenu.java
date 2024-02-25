// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import android.content.Context;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ListView;

import androidx.annotation.ColorRes;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.content.ContextCompat;
import androidx.core.view.ViewCompat;

import org.chromium.ui.R;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.LinkedList;
import java.util.List;

/**
 * An implementation of a list menu. Uses app_menu_layout as the default layout of menu and
 * list_menu_item as the default layout of a menu item.
 */
public class BasicListMenu implements ListMenu, OnItemClickListener {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ListMenuItemType.DIVIDER, ListMenuItemType.MENU_ITEM})
    public @interface ListMenuItemType {
        int DIVIDER = 0;
        int MENU_ITEM = 1;
    }

    /**
     * Helper function to build a ListItem of a divider.
     *
     * @return ListItem Representing a divider.
     */
    public static ListItem buildMenuDivider() {
        return new ListItem(ListMenuItemType.DIVIDER, new PropertyModel());
    }

    /**
     * Helper function to build a list menu item.
     *
     * @param title the title of the menu item.
     * @param contentDescription the a11y content description of the menu item.
     * @param groupId the group ID of the menu item. Pass 0 for none.
     * @param id the ID of the menu item. Pass 0 for none.
     * @param startIcon the start icon of the menu item. Pass 0 for none.
     * @param isIconTintable true if the icon should be tinted with the default tint.
     * @param groupContainsIcon true if the group the item belongs to contains an item with an icon.
     * @param enabled true if the menu item is enabled.
     * @param clickListener the click listener of the menu item.
     * @param intent the intent of the menu item.
     * @return ListItem representing a menu item.
     */
    public static ListItem buildListMenuItem(
            String title,
            @Nullable String contentDescription,
            int groupId,
            int id,
            @Nullable Drawable startIcon,
            boolean isIconTintable,
            boolean groupContainsIcon,
            boolean enabled,
            @Nullable View.OnClickListener clickListener,
            @Nullable Intent intent) {
        PropertyModel.Builder modelBuilder =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TITLE, title)
                        .with(ListMenuItemProperties.CONTENT_DESCRIPTION, contentDescription)
                        .with(ListMenuItemProperties.GROUP_ID, groupId)
                        .with(ListMenuItemProperties.MENU_ITEM_ID, id)
                        .with(ListMenuItemProperties.START_ICON_DRAWABLE, startIcon)
                        .with(ListMenuItemProperties.ENABLED, enabled)
                        .with(ListMenuItemProperties.CLICK_LISTENER, clickListener)
                        .with(ListMenuItemProperties.INTENT, intent)
                        .with(
                                ListMenuItemProperties.KEEP_START_ICON_SPACING_WHEN_HIDDEN,
                                groupContainsIcon)
                        .with(
                                ListMenuItemProperties.TEXT_APPEARANCE_ID,
                                R.style.TextAppearance_ListMenuItem)
                        .with(
                                ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID,
                                isIconTintable ? R.color.list_menu_item_icon_color_list : 0);
        return new ListItem(ListMenuItemType.MENU_ITEM, modelBuilder.build());
    }

    private final @NonNull ListView mListView;
    private final @NonNull ModelListAdapter mAdapter;
    private final @NonNull View mContentView;
    private final @NonNull List<Runnable> mClickRunnables;
    private final @NonNull Delegate mDelegate;

    /**
     * @param context The {@link Context} to inflate the layout.
     * @param data Data representing the list items. All items in data are assumed to be enabled.
     * @param contentView The background of the list menu.
     * @param listView The {@link ListView} of the list menu.
     * @param delegate The {@link Delegate} that would be called when the menu is clicked.
     * @param backgroundTintColor The background tint color of the menu.
     */
    public BasicListMenu(
            @NonNull Context context,
            @NonNull ModelList data,
            @NonNull View contentView,
            @NonNull ListView listView,
            @NonNull Delegate delegate,
            @ColorRes int backgroundTintColor) {
        mAdapter = new ListMenuItemAdapter(data);
        mContentView = contentView;
        mListView = listView;
        mListView.setAdapter(mAdapter);
        mListView.setDivider(null);
        mListView.setOnItemClickListener(this);
        mDelegate = delegate;
        mClickRunnables = new LinkedList<>();
        registerListItemTypes();

        if (backgroundTintColor != 0) {
            ViewCompat.setBackgroundTintList(
                    mContentView,
                    ColorStateList.valueOf(ContextCompat.getColor(context, backgroundTintColor)));
        }
    }

    @NonNull
    @Override
    public View getContentView() {
        return mContentView;
    }

    @NonNull
    public ListView getListView() {
        return mListView;
    }

    @Override
    public void addContentViewClickRunnable(Runnable runnable) {
        mClickRunnables.add(runnable);
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        mDelegate.onItemSelected(((ListItem) mAdapter.getItem(position)).model);
        for (Runnable r : mClickRunnables) {
            r.run();
        }
    }

    @Override
    public int getMaxItemWidth() {
        return UiUtils.computeListAdapterContentDimensions(mAdapter, mListView)[0];
    }

    /**
     * Returns the measured width and height of the menu.
     *
     * @return an array with the menu's width stored at index 0, and the height stored at index 1.
     */
    public int[] getMenuDimensions() {
        int[] result = UiUtils.computeListAdapterContentDimensions(mAdapter, mListView);
        int horizontalPadding = mContentView.getPaddingLeft() + mContentView.getPaddingRight();
        int verticalPadding = mContentView.getPaddingTop() + mContentView.getPaddingBottom();
        result[0] += horizontalPadding;
        result[1] += verticalPadding;
        return result;
    }

    private void registerListItemTypes() {
        mAdapter.registerType(
                ListMenuItemType.MENU_ITEM,
                new LayoutViewBuilder(R.layout.list_menu_item),
                ListMenuItemViewBinder::binder);
        mAdapter.registerType(
                ListMenuItemType.DIVIDER,
                new LayoutViewBuilder(R.layout.list_section_divider),
                ListSectionDividerViewBinder::bind);
    }
}
