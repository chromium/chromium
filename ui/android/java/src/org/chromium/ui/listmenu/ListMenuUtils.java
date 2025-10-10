// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.chromium.ui.base.KeyNavigationUtil.isGoBackward;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.HOVER_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE;
import static org.chromium.ui.listmenu.ListMenuItemProperties.TITLE_ID;
import static org.chromium.ui.listmenu.ListMenuSubmenuHeaderItemProperties.KEY_LISTENER;
import static org.chromium.ui.listmenu.ListMenuSubmenuItemProperties.SUBMENU_ITEMS;

import android.content.Context;
import android.content.res.Resources;
import android.os.SystemClock;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

import androidx.annotation.StringRes;
import androidx.core.view.ViewCompat;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.R;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.ListObservable.ListObserver;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
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

    /**
     * Callback to use when a menu item of type MENU_ITEM_WITH_SUBMENU is clicked.
     *
     * @param headerModelList {@link ModelList} for unscrollable top header; null if headers scroll.
     * @param contentModelList {@link ModelList} for the scrollable content of the menu.
     * @param item The menu item which was clicked.
     * @param drillDownOverrideValue An optional override value. If non-null, we use drilldown if
     *     it's true and flyout if it's false to display submenus. If null, we determine which to
     *     use based on system conditions.
     */
    private static void onItemWithSubmenuClicked(
            @Nullable ModelList headerModelList,
            ModelList contentModelList,
            ListItem item,
            @Nullable Boolean drillDownOverrideValue) {
        if (!ListMenuFlyoutController.shouldUseDrillDown(drillDownOverrideValue)) {
            return;
        }

        @Nullable ModelList parentHeaderModelList =
                headerModelList == null ? null : shallowCopy(headerModelList);
        ModelList parentModelList = shallowCopy(contentModelList);
        // Add the clicked item as a header to the submenu.
        Runnable headerBackClick =
                () -> {
                    if (headerModelList != null && parentHeaderModelList != null) {
                        setModelListContent(headerModelList, parentHeaderModelList);
                    }
                    setModelListContent(contentModelList, parentModelList);
                };
        final PropertyModel model =
                new PropertyModel.Builder(ListMenuSubmenuHeaderItemProperties.ALL_KEYS)
                        .with(TITLE, item.model.get(TITLE))
                        .with(ENABLED, true)
                        .with(CLICK_LISTENER, (unusedView) -> headerBackClick.run())
                        .with(
                                KEY_LISTENER,
                                (view, keyCode, keyEvent) -> {
                                    if (isGoBackward(keyEvent)) {
                                        headerBackClick.run();
                                        return true;
                                    }
                                    // Return false because the listener has not consumed the event.
                                    return false;
                                })
                        .build();
        ListItem headerItem = new ListItem(ListItemType.SUBMENU_HEADER, model);
        List<ListItem> newContentList = new ArrayList<>();
        if (headerModelList == null) {
            newContentList.add(headerItem);
        } else {
            headerModelList.set(List.of(headerItem));
        }
        newContentList.addAll(item.model.get(SUBMENU_ITEMS));
        contentModelList.set(newContentList);
    }

    private static void setModelListContent(ModelList modelList, ModelList target) {
        List<ListItem> targetItems = new ArrayList<>();
        for (ListItem item : target) {
            targetItems.add(item);
        }
        modelList.set(targetItems);
    }

    /** Returns a shallow copy of {@param modelList}. */
    private static ModelList shallowCopy(ModelList modelList) {
        ModelList result = new ModelList();
        for (ListItem item : modelList) {
            result.add(item);
        }
        return result;
    }

    /** Returns whether {@param item} has a click listener. */
    public static boolean hasClickListener(ListItem item) {
        return item.model != null
                && item.model.containsKey(CLICK_LISTENER)
                && item.model.get(CLICK_LISTENER) != null;
    }

    /**
     * Makes {@param dismissDialog} run at the end of the callback of {@param item}. If the item
     * doesn't already have a click callback in its model, no click callback is added.
     *
     * @param item The item to which we would add {@param runnable}.
     * @param dismissDialog The {@link Runnable} to run to dismiss the dialog.
     */
    private static void addRunnableToCallback(ListItem item, Runnable dismissDialog) {
        if (hasClickListener(item)) {
            View.OnClickListener oldListener = item.model.get(CLICK_LISTENER);
            item.model.set(
                    CLICK_LISTENER,
                    (view) -> {
                        oldListener.onClick(view);
                        dismissDialog.run();
                    });
        }
    }

    /**
     * Sets up the necessary callbacks for a menu item and its sub-items, recursively. This includes
     * setting `HOVER_LISTENER` for flyout menus and `CLICK_LISTENER` for drill-down menus. It also
     * attaches the {@param dismissDialog} runnable to the click handlers of terminal items.
     *
     * @param headerModelList {@link ModelList} for unscrollable top header; null if headers scroll.
     * @param contentModelList {@link ModelList} for the scrollable content of the menu.
     * @param item The item to start with.
     * @param dismissDialog The {@link Runnable} to run.
     * @param flyoutController The {@link ListMenuFlyoutController} to manage the popups.
     * @param drillDownOverrideValue An optional override value. If non-null, we use drilldown if
     *     it's true and flyout if it's false to display submenus. If null, this class determines
     *     the appropriate style based on system conditions.
     */
    private static void setupCallbacksRecursivelyForItem(
            @Nullable ModelList headerModelList,
            ModelList contentModelList,
            ListItem item,
            Runnable dismissDialog,
            @Nullable ListMenuFlyoutController flyoutController,
            int levelOfHoveredItem,
            @Nullable Boolean drillDownOverrideValue,
            List<ListItem> ancestorPath) {
        if (item.model == null) return;

        List<ListItem> highlightPath = new ArrayList<ListItem>(ancestorPath);
        highlightPath.add(item);

        // We add `HOVER_LISTENER` to items without submenus too because we might need to dismiss
        // open flyout popups.
        if (!ListMenuFlyoutController.shouldUseDrillDown(drillDownOverrideValue)
                && flyoutController != null
                && item.model.containsKey(HOVER_LISTENER)) {
            item.model.set(
                    HOVER_LISTENER,
                    (view, event) -> {
                        return flyoutController.handleHoverEvent(
                                event,
                                item,
                                view,
                                levelOfHoveredItem,
                                drillDownOverrideValue,
                                highlightPath);
                    });

            View.OnKeyListener originalListener =
                    item.model.get(org.chromium.ui.listmenu.ListMenuItemProperties.KEY_LISTENER);
            item.model.set(
                    org.chromium.ui.listmenu.ListMenuItemProperties.KEY_LISTENER,
                    (view, keyCode, keyEvent) -> {
                        if (isGoBackward(keyEvent)) {
                            flyoutController.exitFlyoutWithoutDelay(
                                    levelOfHoveredItem, view, highlightPath);
                            return true;
                        }

                        if (originalListener != null) {
                            return originalListener.onKey(view, keyCode, keyEvent);
                        }

                        // Return false because the listener has not consumed the event.
                        return false;
                    });
        }

        if (item.model.containsKey(SUBMENU_ITEMS)) {
            final View.OnClickListener existingListener = item.model.get(CLICK_LISTENER);
            item.model.set(
                    CLICK_LISTENER,
                    (view) -> {
                        if (existingListener != null) {
                            existingListener.onClick(view);
                        }
                        if (ListMenuFlyoutController.shouldUseDrillDown(drillDownOverrideValue)) {
                            onItemWithSubmenuClicked(
                                    headerModelList,
                                    contentModelList,
                                    item,
                                    drillDownOverrideValue);
                        } else if (flyoutController != null) {
                            // Allow for controlling flyout with keyboard for accessibility.
                            flyoutController.enterFlyoutWithoutDelay(
                                    item, view, levelOfHoveredItem, highlightPath);
                        }
                    });
            for (ListItem submenuItem :
                    PropertyModel.getFromModelOrDefault(item.model, SUBMENU_ITEMS, List.of())) {
                setupCallbacksRecursivelyForItem(
                        headerModelList,
                        contentModelList,
                        submenuItem,
                        dismissDialog,
                        flyoutController,
                        levelOfHoveredItem + 1,
                        drillDownOverrideValue,
                        highlightPath);
            }
        } else {
            // Note: SUBMENU_HEADER items should be (and are) excluded by this, because
            // SUBMENU_HEADER items aren't in the model's SUBMENU_ITEMS.
            // MENU_ITEM_WITH_SUBMENU items should also not be included.
            // The rationale for excluding these is that we don't want to dismiss the dialog when we
            // are navigating through submenus.
            addRunnableToCallback(item, dismissDialog);
        }
    }

    /**
     * Runs {@param dismissDialog} at the end of each callback, recursively (through submenu items).
     * If an item doesn't already have a click callback in its model, no click callback is added.
     *
     * @param headerModelList {@link ModelList} for unscrollable top header; null if headers scroll.
     * @param contentModelList {@link ModelList} for the scrollable content of the menu.
     * @param dismissDialog The {@link Runnable} to run.
     * @param flyoutController The {@link ListMenuFlyoutController} to manage the flyout popups.
     * @param drillDownOverrideValue An optional override value. If non-null, we use drilldown if
     *     it's true and flyout if it's false to display submenus. If null, this class determines
     *     the appropriate style based on system conditions.
     */
    public static void setupCallbacksRecursively(
            @Nullable ModelList headerModelList,
            ModelList contentModelList,
            Runnable dismissDialog,
            @Nullable ListMenuFlyoutController flyoutController,
            @Nullable Boolean drillDownOverrideValue) {
        long time = SystemClock.elapsedRealtime();
        if (headerModelList != null) {
            for (ListItem listItem : headerModelList) {
                setupCallbacksRecursivelyForItem(
                        headerModelList,
                        contentModelList,
                        listItem,
                        dismissDialog,
                        flyoutController,
                        /* levelOfHoveredItem= */ 0,
                        drillDownOverrideValue,
                        new ArrayList<ListItem>());
            }
        }
        for (ListItem listItem : contentModelList) {
            setupCallbacksRecursivelyForItem(
                    headerModelList,
                    contentModelList,
                    listItem,
                    dismissDialog,
                    flyoutController,
                    /* levelOfHoveredItem= */ 0,
                    drillDownOverrideValue,
                    new ArrayList<ListItem>());
        }
        RecordHistogram.recordTimesHistogram(
                "ListMenuUtils.SetupCallbacksRecursively.Duration",
                SystemClock.elapsedRealtime() - time);
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

    /**
     * Set the focus state for a given content view. This is to make sure that hover navigation,
     * keyboard navigation and the combination of both work for flyout menus. We have to make sure
     * that only the top flyout popup is focused, and that the {@code ListView} in the other popups
     * don't get focus when the device exits touch mode due to e.g. keyboard activity.
     *
     * @param contentView The content view to change the focus settings for.
     * @param hasFocus Whether this content view should have focus.
     */
    public static void setWindowFocus(ViewGroup contentView, boolean hasFocus) {
        if (hasFocus) {
            contentView.setDescendantFocusability(ViewGroup.FOCUS_AFTER_DESCENDANTS);
            contentView.requestFocus();
        } else {
            contentView.setDescendantFocusability(ViewGroup.FOCUS_BLOCK_DESCENDANTS);
            contentView.clearFocus();
        }
    }
}
