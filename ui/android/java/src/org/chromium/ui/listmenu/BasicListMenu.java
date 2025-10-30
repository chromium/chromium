// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static android.view.View.INVISIBLE;
import static android.view.View.VISIBLE;

import static org.chromium.ui.listmenu.ListMenuUtils.createAdapter;

import android.content.Context;
import android.content.Intent;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.DrawableRes;
import androidx.core.content.ContextCompat;
import androidx.core.view.ViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.R;
import org.chromium.ui.UiUtils;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController.AccessibilityListObserver;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.function.Supplier;

/**
 * An implementation of a list menu. Uses app_menu_layout as the default layout of menu and
 * list_menu_item as the default layout of a menu item.
 */
@NullMarked
public class BasicListMenu implements ListMenu {

    /**
     * Helper function to build a ListItem of a divider.
     *
     * @param isIncognito Whether we're creating an incognito-themed menu.
     * @return ListItem Representing a divider.
     */
    public static ListItem buildMenuDivider(boolean isIncognito) {
        PropertyModel.Builder builder =
                new PropertyModel.Builder(ListSectionDividerProperties.ALL_KEYS);
        if (isIncognito) {
            builder.with(
                    ListSectionDividerProperties.COLOR_ID, R.color.divider_line_bg_color_light);
        }
        return new ListItem(ListItemType.DIVIDER, builder.build());
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
            @Nullable Intent intent,
            int order) {
        PropertyModel.Builder modelBuilder =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TITLE, title)
                        .with(ListMenuItemProperties.CONTENT_DESCRIPTION, contentDescription)
                        .with(ListMenuItemProperties.GROUP_ID, groupId)
                        .with(ListMenuItemProperties.MENU_ITEM_ID, id)
                        .with(ListMenuItemProperties.START_ICON_DRAWABLE, startIcon)
                        .with(ListMenuItemProperties.ENABLED, enabled)
                        .with(ListMenuItemProperties.INTENT, intent)
                        .with(
                                ListMenuItemProperties.KEEP_START_ICON_SPACING_WHEN_HIDDEN,
                                groupContainsIcon)
                        .with(
                                ListMenuItemProperties.TEXT_APPEARANCE_ID,
                                R.style.TextAppearance_DensityAdaptive_ListMenuItem)
                        .with(
                                ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID,
                                isIconTintable ? R.color.list_menu_item_icon_color_list : 0)
                        .with(ListMenuItemProperties.ORDER, order);
        return new ListItem(ListItemType.MENU_ITEM, modelBuilder.build());
    }

    private final View mListMenuLayout;

    private final ListView mHeaderListView;
    private final ModelList mHeaderModelList;
    private final ModelListAdapter mHeaderAdapter;

    private final ListView mContentListView;
    private final ModelList mContentModelList;
    private final ModelListAdapter mContentAdapter;

    private final ContentListOnScrollChangeListener mScrollChangeListener;

    private final List<Runnable> mClickRunnables = new ArrayList<>();

    /**
     * @param context The {@link Context} to inflate the layout.
     * @param data Data representing the list items. All items in data are assumed to be enabled.
     * @param delegate The {@link ListMenu.Delegate} used to handle menu clicks. If not provided,
     *     the item's CLICK_LISTENER or listMenu's onMenuItemSelected method will be used.
     * @param backgroundDrawable The {@link DrawableRes} to use as the menu background. If 0, the
     *     default ({@code @drawable/list_menu_background)} will be used.
     * @param backgroundTintColor The background tint color of the menu.
     * @param bottomHairlineColor The {@link ColorInt} to use as the color for the bottom hairline
     *     of the unscrollable header. If -1, the default ({@code ?android:attr/listDivider}) will
     *     be used.
     */
    public BasicListMenu(
            Context context,
            ModelList data,
            @Nullable Delegate delegate,
            @DrawableRes int backgroundDrawable,
            @ColorRes int backgroundTintColor,
            @Nullable @ColorInt Integer bottomHairlineColor) {
        mListMenuLayout = LayoutInflater.from(context).inflate(R.layout.list_menu_layout, null);
        View hairline = mListMenuLayout.findViewById(R.id.menu_header_bottom_hairline);

        mContentModelList = data;
        mContentAdapter =
                createAdapter(data, Set.of(), (model, view) -> callDelegate(delegate, model, view));
        mContentListView = mListMenuLayout.findViewById(R.id.menu_list);
        mContentListView.setAdapter(mContentAdapter);
        mContentListView.setDivider(null);

        mHeaderModelList = new ModelList();
        mHeaderAdapter =
                createAdapter(
                        mHeaderModelList,
                        Set.of(),
                        (model, view) -> callDelegate(delegate, model, view));
        mHeaderListView = mListMenuLayout.findViewById(R.id.menu_header);
        mHeaderListView.setAdapter(mHeaderAdapter);

        // Allow keyboard focus + keyboard clicks on list items.
        mHeaderListView.setItemsCanFocus(true);
        mContentListView.setItemsCanFocus(true);

        if (backgroundDrawable != Resources.ID_NULL) {
            mListMenuLayout.setBackgroundResource(backgroundDrawable);
        }
        if (backgroundTintColor != 0) {
            ViewCompat.setBackgroundTintList(
                    mListMenuLayout,
                    ColorStateList.valueOf(ContextCompat.getColor(context, backgroundTintColor)));
        }
        if (bottomHairlineColor != null) {
            hairline.setBackgroundColor(bottomHairlineColor);
        }

        mScrollChangeListener =
                new ContentListOnScrollChangeListener(hairline, () -> !mHeaderModelList.isEmpty());
        mContentListView.setOnScrollChangeListener(mScrollChangeListener);
    }

    @Override
    public View getContentView() {
        return mListMenuLayout;
    }

    public ListView getListView() {
        return mContentListView;
    }

    @Override
    public void addContentViewClickRunnable(Runnable runnable) {
        mClickRunnables.add(runnable);
    }

    @Override
    public int getMaxItemWidth() {
        return UiUtils.computeListAdapterContentDimensions(mContentAdapter, mContentListView)[0];
    }

    /**
     * Returns the measured width and height of the menu.
     *
     * @return an array with the menu's width stored at index 0, and the height stored at index 1.
     */
    public int[] getMenuDimensions() {
        // Compute vertical size of header + content.
        int[] headerDimensions =
                UiUtils.computeListAdapterContentDimensions(mHeaderAdapter, mHeaderListView);
        int[] contentDimensions =
                UiUtils.computeListAdapterContentDimensions(mContentAdapter, mContentListView);
        // The header is above the content, so the result width is the max of the 2 widths and the
        // result height is the addition of the 2 heights.
        int[] result = {
            Math.max(headerDimensions[0], contentDimensions[0]),
            headerDimensions[1] + contentDimensions[1]
        };
        // Now add padding from the listMenuLayout (which contains the header and content -- note
        // that the header and content don't have padding individually).
        int horizontalPadding =
                mListMenuLayout.getPaddingLeft() + mListMenuLayout.getPaddingRight();
        int verticalPadding = mListMenuLayout.getPaddingTop() + mListMenuLayout.getPaddingBottom();
        result[0] += horizontalPadding;
        result[1] += verticalPadding;
        return result;
    }

    public ModelListAdapter getContentAdapter() {
        return mContentAdapter;
    }

    /**
     * Runs {@param dismissDialog} at the end of each callback, recursively (through submenu items).
     * If an item doesn't already have a click callback in its model, no click callback is added.
     *
     * @param dismissDialog The {@link Runnable} to run.
     * @param hierarchicalMenuController The {@link HierarchicalMenuController} to use.
     */
    public void setupCallbacksRecursively(
            Runnable dismissDialog, HierarchicalMenuController hierarchicalMenuController) {
        AccessibilityListObserver observer =
                hierarchicalMenuController
                .new AccessibilityListObserver(
                        mListMenuLayout,
                        mHeaderListView,
                        mContentListView,
                        mHeaderModelList,
                        mContentModelList);
        mHeaderModelList.addObserver(observer);
        mContentModelList.addObserver(observer);

        hierarchicalMenuController.setupCallbacksRecursively(
                mHeaderModelList, mContentModelList, dismissDialog);
    }

    private void callDelegate(@Nullable Delegate delegate, PropertyModel model, View view) {
        if (delegate != null) delegate.onItemSelected(model, view);
        // We will run the runnables that are registered by the time this lambda
        // is called.
        for (Runnable r : mClickRunnables) {
            r.run();
        }
    }

    /** Listens to scrolls on list view contents and changes visibility of header hairline. */
    private static class ContentListOnScrollChangeListener implements View.OnScrollChangeListener {

        private final View mDivider;
        private final Supplier<Boolean> mShowHairlinePrecondition;
        private int mVisibility = INVISIBLE; // "Cache" so we don't set visibility per scroll event

        /**
         * Creates a {@link ContentListOnScrollChangeListener}.
         *
         * @param divider The divider whose appearance to control.
         * @param showHairlinePrecondition A {@link Supplier}. This is checked before showing the
         *     hairline. If false, hairline should not be shown.
         */
        ContentListOnScrollChangeListener(
                View divider, Supplier<Boolean> showHairlinePrecondition) {
            mDivider = divider;
            mShowHairlinePrecondition = showHairlinePrecondition;
        }

        @Override
        public void onScrollChange(
                View view, int scrollX, int scrollY, int oldScrollX, int oldScrollY) {
            if (view instanceof ListView listView) {
                @Nullable View firstChild = listView.getChildAt(0);
                if (firstChild == null) return;
                // Estimation of list scroll Y, assuming that children are the same height.
                int listScrollY =
                        -firstChild.getTop()
                                + (listView.getFirstVisiblePosition() * firstChild.getHeight());
                int desiredVisibility =
                        (mShowHairlinePrecondition.get() && listScrollY > 0) ? VISIBLE : INVISIBLE;
                if (desiredVisibility != mVisibility) {
                    mVisibility = desiredVisibility;
                    mDivider.setVisibility(desiredVisibility);
                }
            }
        }
    }

    public void clickItemForTesting(int i) {
        mContentAdapter
                .getView(i, new View(mListMenuLayout.getContext()), (ViewGroup) mListMenuLayout)
                .performClick();
    }

    public View.OnScrollChangeListener getScrollChangeListenerForTesting() {
        return mScrollChangeListener;
    }
}
