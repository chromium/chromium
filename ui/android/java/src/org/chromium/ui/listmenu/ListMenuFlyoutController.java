// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.chromium.ui.listmenu.ListMenuItemProperties.IS_HIGHLIGHTED;
import static org.chromium.ui.listmenu.ListMenuSubmenuItemProperties.SUBMENU_ITEMS;

import android.os.Handler;
import android.util.Pair;
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
        switch (event.getAction()) {
            case MotionEvent.ACTION_HOVER_ENTER:
                onItemHovered(
                        item, view, levelOfHoveredItem, drillDownOverrideValue, highlightPath);
                return true;
            case MotionEvent.ACTION_HOVER_EXIT:
                if (item.model.get(IS_HIGHLIGHTED)) {
                    updateHighlightPath(highlightPath.subList(0, highlightPath.size() - 1));
                }
                cancelFlyoutDelay(view);
                // We only want to remove the flyout popups when the user hovers
                // over another item. We don't close the flyout popup even when the
                // item itself loses hover.
                return true;
            default:
                return false;
        }
    }

    private void onItemHovered(
            ListItem item,
            View view,
            int levelOfHoveredItem,
            @Nullable Boolean drillDownOverrideValue,
            List<ListItem> highlightPath) {
        updateHighlightPath(highlightPath);

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

    private void updateHighlightPath(List<ListItem> highlightPath) {
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
        List<Pair<@Nullable ListItem, T>> dialogs = mFlyoutHandler.getFlyoutWindows();

        if (levelOfHoveredItem >= dialogs.size()) {
            return;
        }

        boolean keepChildWindow = false;

        // If child popups exist.
        if (levelOfHoveredItem < dialogs.size() - 1) {
            // We want to keep the direct child open if the hover is still on the same child.
            ListItem parentItemOfCurrentFlyoutPopup = dialogs.get(levelOfHoveredItem + 1).first;
            keepChildWindow = item == parentItemOfCurrentFlyoutPopup;

            int clearFromIndex = keepChildWindow ? levelOfHoveredItem + 2 : levelOfHoveredItem + 1;
            if (clearFromIndex < dialogs.size()) {
                mFlyoutHandler.removeFlyoutWindows(clearFromIndex);
            }
        }

        // Create a new child popup if the item has submenu and we removed the child window.
        if (item.model.containsKey(SUBMENU_ITEMS) && !keepChildWindow) {
            mFlyoutHandler.addFlyoutWindow(item, view);
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
