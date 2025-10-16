// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.hierarchicalmenu;

import android.os.Handler;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.R;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;

import java.util.List;

/**
 * A controller for managing flyout menus in a list-based menu system. This class handles the logic
 * for displaying and dismissing nested submenus (flyouts) based on hover events. It also tracks the
 * currently highlighted menu path and uses a {@link FlyoutHandler} to interact with the actual
 * flyout popup windows.
 *
 * @param <T> The type representing the flyout popup window. This could be a Dialog, PopupWindow, or
 *     any other UI component used to display the submenu.
 */
@NullMarked
public class FlyoutController<T> {
    private final FlyoutHandler<T> mFlyoutHandler;
    private @Nullable Runnable mFlyoutAfterDelayRunnable;
    private @Nullable View mPendingFlyoutParentView;
    private final HierarchicalMenuKeyProvider mKeyProvider;
    private final HierarchicalMenuController mMenuController;

    /**
     * A data class holding a flyout popup window and the (optional) parent ListItem that triggered
     * it. The root popup will have a null parentItem.
     *
     * @param <T> The type of the object representing the flyout popup.
     */
    public static class FlyoutPopupEntry<T> {
        public final @Nullable ListItem parentItem;
        public final T popupWindow;

        public FlyoutPopupEntry(@Nullable ListItem parentItem, T popupWindow) {
            this.parentItem = parentItem;
            this.popupWindow = popupWindow;
        }
    }

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
         * @return A List of {@link FlyoutWindowEntry} objects, each mapping a popup to its parent
         *     item.
         */
        List<FlyoutPopupEntry<T>> getFlyoutWindows();

        /**
         * Adds a flyout popup.
         *
         * @param item The ListItem that got the hover.
         * @param view The View that got the hover.
         * @param levelOfHoveredItem The depth of the item within the menu hierarchy (e.g., 0 for
         *     root items, 1 for sub-menu items).
         */
        void addFlyoutWindow(ListItem item, View view, int levelOfHoveredItem);

        /**
         * Remove popups with indices above removeFromIndex.
         *
         * @param removeFromIndex The minimum index of the popup to be removed.
         */
        void removeFlyoutWindows(int removeFromIndex);
    }

    public FlyoutController(
            FlyoutHandler<T> flyoutHandler,
            HierarchicalMenuKeyProvider keyProvider,
            HierarchicalMenuController menuController) {
        mFlyoutHandler = flyoutHandler;
        mKeyProvider = keyProvider;
        mMenuController = menuController;
    }

    /**
     * Trigger flyout immediately without the delay, e.g. when keyboard is used to navigate flyout
     * menus.
     *
     * @param item The ListItem that is the target of the focus event.
     * @param view The View associated with the hovered ListItem.
     * @param levelOfHoveredItem The depth of the item within the menu hierarchy (e.g., 0 for root
     *     items, 1 for sub-menu items).
     * @param highlightPath The complete list of items from the root of the menu to the currently
     *     hovered {@code item}, inclusive.
     */
    public void enterFlyoutWithoutDelay(
            ListItem item, View view, int levelOfHoveredItem, List<ListItem> highlightPath) {
        mMenuController.updateHighlights(highlightPath);
        cancelFlyoutDelay(view);
        onFlyoutAfterDelay(item, view, levelOfHoveredItem);
    }

    /**
     * Remove flyout windows with levels larger than or equal to {@code clearFromIndex} immediately,
     * e.g. when keyboard is used to navigate flyout menus.
     *
     * @param clearFromIndex The minimum level of flyout popup to remove.
     * @param view The View associated with the hovered ListItem.
     * @param highlightPath The complete list of items from the root of the menu to the currently
     *     hovered {@code item}, inclusive.
     */
    public void exitFlyoutWithoutDelay(
            int clearFromIndex, View view, List<ListItem> highlightPath) {
        // We do not dismiss the main, non-flyout popup here.
        if (clearFromIndex == 0) {
            return;
        }

        cancelFlyoutDelay(view);

        // We need to remove hover from the popup currently in focus.
        mMenuController.updateHighlights(highlightPath.subList(0, clearFromIndex - 1));

        mFlyoutHandler.removeFlyoutWindows(clearFromIndex);
    }

    /**
     * Starts a timer that removes and adds flyout popups.
     *
     * @param item The hovered item.
     * @param view The hovered view.
     * @param levelOfHoveredItem The depth of the item within the menu hierarchy (e.g., 0 for root
     *     items, 1 for sub-menu items).
     * @param highlightPath The complete list of items from the root of the menu to the currently
     *     hovered {@code item}, inclusive.
     */
    public void onItemHovered(
            ListItem item, View view, int levelOfHoveredItem, List<ListItem> highlightPath) {
        // Since we received a new `HOVER` event, we cancel the previous timer.
        cancelFlyoutDelay(view);

        // We wait for a set period of time before we go on with the UI changes to ensure user
        // intent.
        mFlyoutAfterDelayRunnable =
                () -> {
                    onFlyoutAfterDelay(item, view, levelOfHoveredItem);
                };
        mPendingFlyoutParentView = view;
        Handler handler = view.getHandler();
        assert handler != null;
        handler.postDelayed(
                mFlyoutAfterDelayRunnable,
                view.getContext().getResources().getInteger(R.integer.flyout_menu_delay_in_ms));
    }

    private void onFlyoutAfterDelay(ListItem item, View view, int levelOfHoveredItem) {
        List<FlyoutPopupEntry<T>> dialogs = mFlyoutHandler.getFlyoutWindows();

        if (levelOfHoveredItem >= dialogs.size()) {
            return;
        }

        boolean keepChildWindow = false;

        // If child popups exist.
        if (levelOfHoveredItem < dialogs.size() - 1) {
            // We want to keep the direct child open if the hover is still on the same child.
            FlyoutPopupEntry<T> currentFlyoutPopupEntry = dialogs.get(levelOfHoveredItem + 1);
            keepChildWindow = item == currentFlyoutPopupEntry.parentItem;

            int clearFromIndex = keepChildWindow ? levelOfHoveredItem + 2 : levelOfHoveredItem + 1;
            if (clearFromIndex < dialogs.size()) {
                mFlyoutHandler.removeFlyoutWindows(clearFromIndex);
            }
        }

        // Create a new child popup if the item has submenu and we removed the child window.
        if (item.model.containsKey(mKeyProvider.getSubmenuItemsKey()) && !keepChildWindow) {
            mFlyoutHandler.addFlyoutWindow(item, view, levelOfHoveredItem);
        }
    }

    /**
     * Cancels the timer that was supposed to remove or add flyout popups.
     *
     * @param view The hovered view.
     */
    public void cancelFlyoutDelay(View view) {
        if (mFlyoutAfterDelayRunnable != null && mPendingFlyoutParentView == view) {
            Handler handler = view.getHandler();
            if (handler != null) {
                handler.removeCallbacks(mFlyoutAfterDelayRunnable);
            }
            mFlyoutAfterDelayRunnable = null;
            mPendingFlyoutParentView = null;
        }
    }
}
