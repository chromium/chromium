// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.hierarchicalmenu;

import static org.chromium.ui.base.KeyNavigationUtil.isGoBackward;

import android.os.SystemClock;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.hierarchicalmenu.FlyoutController.FlyoutHandler;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/**
 * A controller to manage the logic for hierarchical menus i.e., flyout and drilldown.
 *
 * <p>This class centralizes the logic for handling submenu interactions. It uses a {@link
 * HierarchicalMenuKeyProvider} to interact with a menu's PropertyModel.
 *
 * @param <T> The type of the object that the {@link FlyoutHandler} manages (e.g., PopupWindow).
 */
@NullMarked
public class HierarchicalMenuController<T> {
    private final @Nullable FlyoutController<T> mFlyoutController;
    private final HierarchicalMenuKeyProvider mKeyProvider;

    /**
     * Creates an instance of the controller.
     *
     * @param keyProvider The {@link HierarchicalMenuKeyProvider} for the controller to use.
     * @param flyoutHandler The {@link FlyoutHandler} for the controller to use for displaying
     *     flyout popups.
     */
    public HierarchicalMenuController(
            HierarchicalMenuKeyProvider keyProvider, @Nullable FlyoutHandler<T> flyoutHandler) {
        mFlyoutController =
                flyoutHandler != null ? new FlyoutController<T>(flyoutHandler, keyProvider) : null;
        mKeyProvider = keyProvider;
    }

    /**
     * Gets the {@link FlyoutController}.
     *
     * @return The {@link FlyoutController} that this controller manages.
     */
    public @Nullable FlyoutController<T> getFlyoutController() {
        return mFlyoutController;
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
    private void onItemWithSubmenuClicked(
            @Nullable ModelList headerModelList,
            ModelList contentModelList,
            ListItem item,
            @Nullable Boolean drillDownOverrideValue) {
        if (!FlyoutController.shouldUseDrillDown(drillDownOverrideValue)) {
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
                new PropertyModel.Builder(mKeyProvider.getAllHeaderItemKeys())
                        .with(
                                mKeyProvider.getTitleKey(),
                                item.model.get(mKeyProvider.getTitleKey()))
                        .with(mKeyProvider.getEnabledKey(), true)
                        .with(
                                mKeyProvider.getClickListenerKey(),
                                (unusedView) -> headerBackClick.run())
                        .with(
                                mKeyProvider.getKeyListenerKey(),
                                (view, keyCode, keyEvent) -> {
                                    if (isGoBackward(keyEvent)) {
                                        headerBackClick.run();
                                        return true;
                                    }
                                    // Return false because the listener has not consumed the event.
                                    return false;
                                })
                        .build();
        ListItem headerItem = new ListItem(mKeyProvider.getSubmenuHeaderType(), model);
        List<ListItem> newContentList = new ArrayList<>();
        if (headerModelList == null) {
            newContentList.add(headerItem);
        } else {
            headerModelList.set(List.of(headerItem));
        }
        newContentList.addAll(item.model.get(mKeyProvider.getSubmenuItemsKey()));
        contentModelList.set(newContentList);
    }

    private void setModelListContent(ModelList modelList, ModelList target) {
        List<ListItem> targetItems = new ArrayList<>();
        for (ListItem item : target) {
            targetItems.add(item);
        }
        modelList.set(targetItems);
    }

    /** Returns a shallow copy of {@param modelList}. */
    private ModelList shallowCopy(ModelList modelList) {
        ModelList result = new ModelList();
        for (ListItem item : modelList) {
            result.add(item);
        }
        return result;
    }

    /** Returns whether {@param item} has a click listener. */
    public boolean hasClickListener(ListItem item) {
        return item.model != null
                && item.model.containsKey(mKeyProvider.getClickListenerKey())
                && item.model.get(mKeyProvider.getClickListenerKey()) != null;
    }

    /**
     * Makes {@param dismissDialog} run at the end of the callback of {@param item}. If the item
     * doesn't already have a click callback in its model, no click callback is added.
     *
     * @param item The item to which we would add {@param runnable}.
     * @param dismissDialog The {@link Runnable} to run to dismiss the dialog.
     */
    private void addRunnableToCallback(ListItem item, Runnable dismissDialog) {
        if (hasClickListener(item)) {
            View.OnClickListener oldListener = item.model.get(mKeyProvider.getClickListenerKey());
            item.model.set(
                    mKeyProvider.getClickListenerKey(),
                    (view) -> {
                        oldListener.onClick(view);
                        dismissDialog.run();
                    });
        }
    }

    /**
     * Sets up the necessary callbacks for a menu item and its sub-items, recursively. This includes
     * setting hover listener for flyout menus and click listener for drill-down menus. It also
     * attaches the {@param dismissDialog} runnable to the click handlers of terminal items.
     *
     * @param headerModelList {@link ModelList} for unscrollable top header; null if headers scroll.
     * @param contentModelList {@link ModelList} for the scrollable content of the menu.
     * @param item The item to start with.
     * @param dismissDialog The {@link Runnable} to run.
     * @param drillDownOverrideValue An optional override value. If non-null, we use drilldown if
     *     it's true and flyout if it's false to display submenus. If null, this class determines
     *     the appropriate style based on system conditions.
     */
    private void setupCallbacksRecursivelyForItem(
            @Nullable ModelList headerModelList,
            ModelList contentModelList,
            ListItem item,
            Runnable dismissDialog,
            int levelOfHoveredItem,
            @Nullable Boolean drillDownOverrideValue,
            List<ListItem> ancestorPath) {
        if (item.model == null) return;

        List<ListItem> highlightPath = new ArrayList<ListItem>(ancestorPath);
        highlightPath.add(item);

        // We add hover listener to items without submenus too because we might need to dismiss
        // open flyout popups.
        if (mFlyoutController != null
                && item.model.containsKey(mKeyProvider.getHoverListenerKey())) {
            item.model.set(
                    mKeyProvider.getHoverListenerKey(),
                    (view, event) -> {
                        return mFlyoutController.handleHoverEvent(
                                event,
                                item,
                                view,
                                levelOfHoveredItem,
                                drillDownOverrideValue,
                                highlightPath);
                    });

            View.OnKeyListener originalListener = item.model.get(mKeyProvider.getKeyListenerKey());
            item.model.set(
                    mKeyProvider.getKeyListenerKey(),
                    (view, keyCode, keyEvent) -> {
                        if (isGoBackward(keyEvent)) {
                            mFlyoutController.exitFlyoutWithoutDelay(
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

        if (item.model.containsKey(mKeyProvider.getSubmenuItemsKey())) {
            final View.OnClickListener existingListener =
                    item.model.get(mKeyProvider.getClickListenerKey());
            item.model.set(
                    mKeyProvider.getClickListenerKey(),
                    (view) -> {
                        if (existingListener != null) {
                            existingListener.onClick(view);
                        }
                        if (FlyoutController.shouldUseDrillDown(drillDownOverrideValue)) {
                            onItemWithSubmenuClicked(
                                    headerModelList,
                                    contentModelList,
                                    item,
                                    drillDownOverrideValue);
                        } else if (mFlyoutController != null) {
                            // Allow for controlling flyout with keyboard for accessibility.
                            mFlyoutController.enterFlyoutWithoutDelay(
                                    item, view, levelOfHoveredItem, highlightPath);
                        }
                    });
            for (ListItem submenuItem :
                    PropertyModel.getFromModelOrDefault(
                            item.model, mKeyProvider.getSubmenuItemsKey(), List.of())) {
                setupCallbacksRecursivelyForItem(
                        headerModelList,
                        contentModelList,
                        submenuItem,
                        dismissDialog,
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
     * @param drillDownOverrideValue An optional override value. If non-null, we use drilldown if
     *     it's true and flyout if it's false to display submenus. If null, this class determines
     *     the appropriate style based on system conditions.
     */
    public void setupCallbacksRecursively(
            @Nullable ModelList headerModelList,
            ModelList contentModelList,
            Runnable dismissDialog,
            @Nullable Boolean drillDownOverrideValue) {
        long time = SystemClock.elapsedRealtime();
        if (headerModelList != null) {
            for (ListItem listItem : headerModelList) {
                setupCallbacksRecursivelyForItem(
                        headerModelList,
                        contentModelList,
                        listItem,
                        dismissDialog,
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
                    /* levelOfHoveredItem= */ 0,
                    drillDownOverrideValue,
                    new ArrayList<ListItem>());
        }
        RecordHistogram.recordTimesHistogram(
                "ListMenuUtils.SetupCallbacksRecursively.Duration",
                SystemClock.elapsedRealtime() - time);
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
    public static void setWindowFocusForFlyoutMenus(ViewGroup contentView, boolean hasFocus) {
        if (hasFocus) {
            contentView.setDescendantFocusability(ViewGroup.FOCUS_AFTER_DESCENDANTS);
            contentView.requestFocus();
        } else {
            contentView.setDescendantFocusability(ViewGroup.FOCUS_BLOCK_DESCENDANTS);
            contentView.clearFocus();
        }
    }
}
