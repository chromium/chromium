// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.chromium.ui.listmenu.ListMenuItemProperties.IS_HIGHLIGHTED;
import static org.chromium.ui.listmenu.ListMenuSubmenuItemProperties.SUBMENU_ITEMS;

import android.os.Handler;
import android.view.MotionEvent;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.R;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;

import java.util.ArrayList;
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
public class ListMenuFlyoutController<T> {
    private final FlyoutHandler<T> mFlyoutHandler;
    private @Nullable Runnable mFlyoutAfterDelayRunnable;
    private @Nullable View mPendingFlyoutParentView;
    private List<ListItem> mLastHighlightedPath = new ArrayList<ListItem>();

    private @Nullable Handler mHoverExitDelayHandler;
    private @Nullable Runnable mPendingHoverExitRunnable;

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

    public ListMenuFlyoutController(FlyoutHandler<T> flyoutHandler) {
        mFlyoutHandler = flyoutHandler;
    }

    /**
     * Processes hover events for a menu item, managing highlight states and flyout menu triggers.
     * This method should be called from an OnHoverListener.
     *
     * <p>On ACTION_HOVER_ENTER, it initiates the logic to highlight the item and potentially show a
     * flyout submenu after a delay.
     *
     * <p>On ACTION_HOVER_EXIT, it updates the highlight path and cancels any pending flyout, but
     * intentionally leaves existing flyout menus open until the user hovers over a new item.
     *
     * @param event The MotionEvent triggered by the hover.
     * @param item The ListItem that is the target of the hover event.
     * @param view The View associated with the hovered ListItem.
     * @param levelOfHoveredItem The depth of the item within the menu hierarchy (e.g., 0 for root
     *     items, 1 for sub-menu items).
     * @param drillDownOverrideValue If not null, forces the menu behavior to be drill-down ({@code
     *     true}) or flyout ({@code false}), overriding the default.
     * @param highlightPath The complete list of items from the root of the menu to the currently
     *     hovered {@code item}, inclusive.
     * @return {@code true} if the hover event was handled (ACTION_HOVER_ENTER or
     *     ACTION_HOVER_EXIT), {@code false} otherwise.
     */
    public boolean handleHoverEvent(
            MotionEvent event,
            ListItem item,
            View view,
            int levelOfHoveredItem,
            @Nullable Boolean drillDownOverrideValue,
            List<ListItem> highlightPath) {
        if (mPendingHoverExitRunnable != null) {
            assert mHoverExitDelayHandler != null;
            mHoverExitDelayHandler.removeCallbacks(mPendingHoverExitRunnable);
            mPendingHoverExitRunnable = null;
            mHoverExitDelayHandler = null;
        }

        switch (event.getAction()) {
            case MotionEvent.ACTION_HOVER_ENTER:
                onItemHovered(
                        item, view, levelOfHoveredItem, drillDownOverrideValue, highlightPath);
                return true;
            case MotionEvent.ACTION_HOVER_EXIT:
                // Update highlights after a short delay. This is to prevent UI flicker when the
                // user moves the pointer from the parent item view to a flyout item view. We
                // receive an {@code ACTION_HOVER_EXIT} event to the parent view right before we
                // receive an {@code ACTION_HOVER_ENTER} event on the flyout view. If we faithfully
                // follow these, the parent item momentarily loses the hover style, so we ignore the
                // first exit event in case it's immediately followed by an enter event.
                cancelFlyoutDelay(view);
                mPendingHoverExitRunnable =
                        () -> {
                            if (item.model.get(IS_HIGHLIGHTED)) {
                                updateHighlights(
                                        highlightPath.subList(0, highlightPath.size() - 1));
                            }
                            mPendingHoverExitRunnable = null;
                        };
                mHoverExitDelayHandler = view.getHandler();
                assert mHoverExitDelayHandler != null;
                mHoverExitDelayHandler.postDelayed(
                        mPendingHoverExitRunnable,
                        view.getContext()
                                .getResources()
                                .getInteger(R.integer.flyout_menu_hover_exit_delay_in_ms));

                // We only want to remove the flyout popups when the user hovers
                // over another item. We don't close the flyout popup even when the
                // item itself loses hover.
                return true;
            default:
                return false;
        }
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
        updateHighlights(highlightPath);
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
        updateHighlights(highlightPath.subList(0, clearFromIndex - 1));

        mFlyoutHandler.removeFlyoutWindows(clearFromIndex);
    }

    private void onItemHovered(
            ListItem item,
            View view,
            int levelOfHoveredItem,
            @Nullable Boolean drillDownOverrideValue,
            List<ListItem> highlightPath) {
        updateHighlights(highlightPath);

        if (shouldUseDrillDown(drillDownOverrideValue)) {
            return;
        }

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

    /**
     * Updates the highlight state of menu items based on the new hover path. The addition of flyout
     * windows requires us to precisely control the hover states of the items. Specifically, when
     * the user is hovering on an item inside a flyout popup, all of the ancestor items should
     * remain highlighted, even when the hover itself is not on those items.
     *
     * @param highlightPath The list of {@link ListItem}s from the root to the currently hovered
     *     item that should be highlighted.
     */
    private void updateHighlights(List<ListItem> highlightPath) {
        int forkIndex = -1;

        for (int i = 0; i < Math.min(mLastHighlightedPath.size(), highlightPath.size()); i++) {
            if (mLastHighlightedPath.get(i) == highlightPath.get(i)) {
                forkIndex = i;
            } else {
                break;
            }
        }

        for (int i = forkIndex + 1; i < mLastHighlightedPath.size(); i++) {
            mLastHighlightedPath.get(i).model.set(IS_HIGHLIGHTED, false);
        }

        for (int i = forkIndex + 1; i < highlightPath.size(); i++) {
            highlightPath.get(i).model.set(IS_HIGHLIGHTED, true);
        }

        mLastHighlightedPath = highlightPath;
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
        if (item.model.containsKey(SUBMENU_ITEMS) && !keepChildWindow) {
            mFlyoutHandler.addFlyoutWindow(item, view, levelOfHoveredItem);
        }
    }

    private void cancelFlyoutDelay(View view) {
        if (mFlyoutAfterDelayRunnable != null && mPendingFlyoutParentView == view) {
            Handler handler = view.getHandler();
            if (handler != null) {
                handler.removeCallbacks(mFlyoutAfterDelayRunnable);
            }
            mFlyoutAfterDelayRunnable = null;
            mPendingFlyoutParentView = null;
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
    public static boolean shouldUseDrillDown(@Nullable Boolean drillDownOverrideValue) {
        if (drillDownOverrideValue != null) {
            return drillDownOverrideValue;
        }

        // TODO(http://crbug.com/440938039): Return `false` when conditions qualify for flyout.
        return true;
    }
}
