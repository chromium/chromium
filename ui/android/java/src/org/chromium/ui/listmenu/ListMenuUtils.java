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

import android.content.res.Resources;
import android.os.Handler;
import android.os.SystemClock;
import android.util.Pair;
import android.view.MotionEvent;
import android.view.View;
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

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Set;

@NullMarked
public class ListMenuUtils {
    private static @Nullable Runnable sFlyoutAfterDelayRunnable;
    private static @Nullable WeakReference<View> sPendingFlyoutParentView;

    /**
     * Defines a contract for managing a series of flyout popups, typically used for nested context
     * menus. An implementing class is responsible for the lifecycle of these popups, including
     * their creation, tracking, and dismissal as the user navigates the menu hierarchy.
     *
     * @param <T> The type of the object representing the flyout popup. This is generic to allow the
     *     implementation to use any UI component.
     */
    public interface FlyoutHandler<T> {
        /**
         * Returns the list of the dialogs, along with the parent ListItem.
         *
         * @return A List of pairs of the parent ListItems and their corresponding dialog popups of
         *     type T.
         */
        List<Pair<@Nullable ListItem, T>> getFlyoutWindows();

        /**
         * Adds a flyout popup.
         *
         * @param item The ListItem that got the hover.
         * @param view The View that got the hover.
         */
        void addFlyoutWindow(ListItem item, View view);

        /**
         * Remove popups with indices above removeFromIndex.
         *
         * @param removeFromIndex The minimum index of the popup to be removed.
         */
        void removeFlyoutWindows(int removeFromIndex);
    }

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
        if (!shouldUseDrillDown(drillDownOverrideValue)) {
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

    private static void onItemWithSubmenuHovered(
            ListItem item,
            View view,
            FlyoutHandler flyoutHandler,
            int levelOfHoveredItem,
            @Nullable Boolean drillDownOverrideValue) {
        if (shouldUseDrillDown(drillDownOverrideValue)) {
            return;
        }

        // Since we received a new `HOVER` event, we cancel the previous timer.
        cancelFlyoutDelay(view);

        // We wait for a set period of time before we go on with the UI changes to ensure user
        // intent.
        sFlyoutAfterDelayRunnable =
                () -> {
                    onFlyoutAfterDelay(item, view, flyoutHandler, levelOfHoveredItem);
                };
        sPendingFlyoutParentView = new WeakReference<>(view);
        Handler handler = view.getHandler();
        assert handler != null;
        handler.postDelayed(
                sFlyoutAfterDelayRunnable,
                view.getContext().getResources().getInteger(R.integer.flyout_menu_delay_in_ms));
    }

    private static void onFlyoutAfterDelay(
            ListItem item, View view, FlyoutHandler flyoutHandler, int levelOfHoveredItem) {
        List<Pair<@Nullable ListItem, ?>> dialogs = flyoutHandler.getFlyoutWindows();

        if (levelOfHoveredItem >= dialogs.size()) {
            return;
        }

        boolean keepChildWindow = false;

        // If child popups exist.
        if (levelOfHoveredItem < dialogs.size() - 1) {
            // We want to keep the direct child open if the hover is still on the same child.
            ListItem parentItemOfCurrentFlyoutPopup = dialogs.get(levelOfHoveredItem + 1).first;
            keepChildWindow = item == parentItemOfCurrentFlyoutPopup;
            flyoutHandler.removeFlyoutWindows(
                    keepChildWindow ? levelOfHoveredItem + 2 : levelOfHoveredItem + 1);
        }

        // Create a new child popup if the item has submenu and we removed the child window.
        if (item.model.containsKey(SUBMENU_ITEMS) && !keepChildWindow) {
            flyoutHandler.addFlyoutWindow(item, view);
        }
    }

    private static void cancelFlyoutDelay(View view) {
        View pendingView =
                (sPendingFlyoutParentView == null) ? null : sPendingFlyoutParentView.get();
        if (sFlyoutAfterDelayRunnable != null && pendingView == view) {
            Handler handler = view.getHandler();
            if (handler != null) {
                handler.removeCallbacks(sFlyoutAfterDelayRunnable);
            }
            sFlyoutAfterDelayRunnable = null;
            sPendingFlyoutParentView = null;
        }
    }

    /**
     * Determines whether to use a drill-down menu style. Currently defaults to using drilldown
     * unless an override value is given.
     *
     * @param drillDownOverrideValue An optional override value. If non-null, this value is returned
     *     directly. If null, the method determines the appropriate style based on system
     *     conditions.
     * @return True to use the drill-down style, false to use the flyout style.
     */
    private static boolean shouldUseDrillDown(@Nullable Boolean drillDownOverrideValue) {
        if (drillDownOverrideValue != null) {
            return drillDownOverrideValue;
        }

        // TODO(http://crbug.com/440938039): Return `false` when conditions qualify for flyout.
        return true;
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
     * @param flyoutHandler The {@link FlyoutHandler} to manage the popups.
     * @param drillDownOverrideValue An optional override value. If non-null, we use drilldown if
     *     it's true and flyout if it's false to display submenus. If null, this class determines
     *     the appropriate style based on system conditions.
     */
    private static void setupCallbacksRecursivelyForItem(
            @Nullable ModelList headerModelList,
            ModelList contentModelList,
            ListItem item,
            Runnable dismissDialog,
            @Nullable FlyoutHandler flyoutHandler,
            int levelOfHoveredItem,
            @Nullable Boolean drillDownOverrideValue) {
        if (item.model == null) return;

        // We add `HOVER_LISTENER` to items without submenus too because we might need to dismiss
        // open flyout popups.
        if (flyoutHandler != null && item.model.containsKey(HOVER_LISTENER)) {
            item.model.set(
                    HOVER_LISTENER,
                    (view, event) -> {
                        switch (event.getAction()) {
                            case MotionEvent.ACTION_HOVER_ENTER:
                                onItemWithSubmenuHovered(
                                        item,
                                        view,
                                        flyoutHandler,
                                        levelOfHoveredItem,
                                        drillDownOverrideValue);
                                break;
                            case MotionEvent.ACTION_HOVER_EXIT:
                                cancelFlyoutDelay(view);
                                // We only want to remove the flyout popups when the user hovers
                                // over another item. We don't close the flyout popup even when the
                                // item itself loses hover.
                                break;
                            default:
                                break;
                        }
                        return false;
                    });
        }

        if (item.model.containsKey(SUBMENU_ITEMS)) {
            item.model.set(
                    CLICK_LISTENER,
                    (unusedView) ->
                            onItemWithSubmenuClicked(
                                    headerModelList,
                                    contentModelList,
                                    item,
                                    drillDownOverrideValue));
            for (ListItem submenuItem :
                    PropertyModel.getFromModelOrDefault(item.model, SUBMENU_ITEMS, List.of())) {
                setupCallbacksRecursivelyForItem(
                        headerModelList,
                        contentModelList,
                        submenuItem,
                        dismissDialog,
                        flyoutHandler,
                        levelOfHoveredItem + 1,
                        drillDownOverrideValue);
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
     * @param flyoutHandler The {@link FlyoutHandler} to manage the popups.
     * @param drillDownOverrideValue An optional override value. If non-null, we use drilldown if
     *     it's true and flyout if it's false to display submenus. If null, this class determines
     *     the appropriate style based on system conditions.
     */
    public static void setupCallbacksRecursively(
            @Nullable ModelList headerModelList,
            ModelList contentModelList,
            Runnable dismissDialog,
            @Nullable FlyoutHandler flyoutHandler,
            @Nullable Boolean drillDownOverrideValue) {
        long time = SystemClock.elapsedRealtime();
        if (headerModelList != null) {
            for (ListItem listItem : headerModelList) {
                setupCallbacksRecursivelyForItem(
                        headerModelList,
                        contentModelList,
                        listItem,
                        dismissDialog,
                        flyoutHandler,
                        /* levelOfHoveredItem= */ 0,
                        drillDownOverrideValue);
            }
        }
        for (ListItem listItem : contentModelList) {
            setupCallbacksRecursivelyForItem(
                    headerModelList,
                    contentModelList,
                    listItem,
                    dismissDialog,
                    flyoutHandler,
                    /* levelOfHoveredItem= */ 0,
                    drillDownOverrideValue);
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
        private final @Nullable ModelList mHeaderModelList;
        private final ModelList mContentModelList;

        /**
         * Returns a {@link AccessibilityListObserver} that reacts to changes in {@param
         * headerModelList and {@param contentModelList}, the are backing models for {@param view}.
         */
        public AccessibilityListObserver(
                View view, @Nullable ModelList headerModelList, ModelList contentModelList) {
            mView = view;
            mHeaderModelList = headerModelList;
            mContentModelList = contentModelList;
        }

        // Note: because ListMenuUtils methods use ModelList#set, they trigger onItemRangeChanged.
        @Override
        public void onItemRangeChanged(
                ListObservable<Void> source, int index, int count, @Nullable Void payload) {
            if (index != 0) return; // If the 1st element wasn't changed, the "header" is the same.
            String accessibilityPaneTitle =
                    mView.getContext().getString(R.string.listmenu_a11y_default_pane_title);
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
                                mView.getContext().getString(firstListItem.model.get(TITLE_ID));
                    }
                }
            }
            ViewCompat.setAccessibilityPaneTitle(mView, accessibilityPaneTitle);
        }
    }
}
