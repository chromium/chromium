// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import android.content.Context;
import android.content.res.ColorStateList;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ListView;

import androidx.annotation.ColorRes;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
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
        return UiUtils.computeMaxWidthOfListAdapterItems(mAdapter, mListView);
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
