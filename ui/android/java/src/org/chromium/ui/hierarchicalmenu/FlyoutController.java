// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.hierarchicalmenu;

import android.graphics.Rect;
import android.os.Handler;
import android.view.View;

import org.chromium.base.lifetime.Destroyable;
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
public class FlyoutController<T> implements Destroyable {

    private final HierarchicalMenuController mMenuController;
    private final HierarchicalMenuKeyProvider mKeyProvider;
    private final FlyoutHandler<T> mFlyoutHandler;

    private final List<FlyoutPopupEntry<T>> mPopups;

    private @Nullable Runnable mFlyoutAfterDelayRunnable;
    private @Nullable View mPendingFlyoutParentView;

    /**
     * A data class holding a flyout popup window and the (optional) parent ListItem that triggered
     * it. The root popup will have a null parentItem.
     *
     * @param <T> The type of the object representing the flyout popup.
     */
    static class FlyoutPopupEntry<T> {
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
         * Gets the coordinates of a given popup window, relative to the main application window.
         *
         * @param popupWindow The popup window instance.
         * @return The {@link Rect} representing the popup's position and size.
         */
        Rect getPopupRect(T popupWindow);

        /**
         * Dismiss the given popup window.
         *
         * @param popupWindow The popup window instance.
         */
        void dismissPopup(T popupWindow);

        /**
         * Set focus state to a given window.
         *
         * @param popupWindow The popup window instance.
         * @param hasFocus Whether the window should have focus.
         */
        void setWindowFocus(T popupWindow, boolean hasFocus);

        /**
         * Creates and shows a flyout popup.
         *
         * @param item The ListItem that got the hover.
         * @param view The View that got the hover.
         * @param dismissRunnable Runnable to run when the window is dismissed.
         * @return The created popup of type {@link T}.
         */
        T createAndShowFlyoutPopup(ListItem item, View view, Runnable dismissRunnable);

        /**
         * Callback triggered after one or more flyout popups are removed.
         *
         * @param removeFromIndex The minimum index of the removed popups.
         */
        default void afterFlyoutPopupsRemoved(int removeFromIndex) {}
    }

    public FlyoutController(
            FlyoutHandler<T> flyoutHandler,
            HierarchicalMenuKeyProvider keyProvider,
            T mainPopup,
            HierarchicalMenuController menuController) {
        mFlyoutHandler = flyoutHandler;
        mKeyProvider = keyProvider;
        mMenuController = menuController;

        mPopups = new ArrayList();
        mPopups.add(new FlyoutPopupEntry(null, mainPopup));
    }

    /**
     * Returns the main popup window.
     *
     * @return The main popup window.
     */
    public T getMainPopup() {
        assert mPopups.size() > 0;
        return mPopups.get(0).popupWindow;
    }

    /**
     * Returns the number of open popups.
     *
     * @return The number of popups, including the main popup.
     */
    public int getNumberOfPopups() {
        return mPopups.size();
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

        removeFlyoutWindows(clearFromIndex);
    }

    private void removeFlyoutWindows(int clearFromIndex) {
        // To dismiss all popups, use {@link #destroy()}.
        assert clearFromIndex > 0;

        if (clearFromIndex >= mPopups.size()) {
            return;
        }

        for (int i = mPopups.size() - 1; i >= clearFromIndex; i--) {
            mFlyoutHandler.dismissPopup(mPopups.get(i).popupWindow);
        }

        mPopups.subList(clearFromIndex, mPopups.size()).clear();

        if (mPopups.size() > 0) {
            mFlyoutHandler.setWindowFocus(mPopups.get(mPopups.size() - 1).popupWindow, true);
        }
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
        if (levelOfHoveredItem >= mPopups.size()) {
            return;
        }

        boolean keepChildWindow = false;

        // If child popups exist.
        if (levelOfHoveredItem < mPopups.size() - 1) {
            // We want to keep the direct child open if the hover is still on the same child.
            FlyoutPopupEntry<T> currentFlyoutPopupEntry = mPopups.get(levelOfHoveredItem + 1);
            keepChildWindow = item == currentFlyoutPopupEntry.parentItem;

            int clearFromIndex = keepChildWindow ? levelOfHoveredItem + 2 : levelOfHoveredItem + 1;
            if (clearFromIndex < mPopups.size()) {
                removeFlyoutWindows(clearFromIndex);
            }
        }

        // Create a new child popup if the item has submenu and we removed the child window.
        if (item.model.containsKey(mKeyProvider.getSubmenuItemsKey()) && !keepChildWindow) {
            T popup =
                    mFlyoutHandler.createAndShowFlyoutPopup(
                            item,
                            view,
                            () -> {
                                removeFlyoutWindows(levelOfHoveredItem + 1);
                            });
            mPopups.add(new FlyoutPopupEntry<T>(null, popup));

            assert mPopups.size() > 1;
            mFlyoutHandler.setWindowFocus(mPopups.get(mPopups.size() - 2).popupWindow, false);
            mFlyoutHandler.setWindowFocus(popup, true);
        }
    }

    /**
     * Gets the {@link Rect} of the main (non-flyout) popup of the menu.
     *
     * @return The rect of the main popup.
     */
    public Rect getMainPopupRect() {
        assert mPopups.size() > 0;
        return mFlyoutHandler.getPopupRect(mPopups.get(0).popupWindow);
    }

    /**
     * Calculate the rect for flyout popups to anchor to, which is useful in case the flyout popup
     * uses {@code AnchoredPopupWindow}. The rect represents the bounds of the {@code itemView}
     * relative to the {@code rootView}, adjusted with the overlap.
     *
     * @param itemView The {@link View} that triggers the flyout menu (e.g., the list item).
     * @param rootView The root {@link View} of the window/popup, used to calculate the relative
     *     coordinates.
     * @return A new {@link Rect} defining the anchor bounds for the flyout popup.
     */
    public static Rect calculateFlyoutAnchorRect(View itemView, View rootView) {
        int[] result = new int[2];
        itemView.getLocationOnScreen(result);

        int[] rootCoordinates = new int[2];
        rootView.getLocationOnScreen(rootCoordinates);

        int horizontalOverlap =
                itemView.getContext()
                        .getResources()
                        .getDimensionPixelSize(
                                R.dimen.hierarchical_menu_flyout_popup_horizontal_overlap);

        return new Rect(
                result[0] - rootCoordinates[0] + horizontalOverlap,
                result[1] - rootCoordinates[1],
                result[0] - rootCoordinates[0] + itemView.getWidth() - horizontalOverlap,
                result[1] - rootCoordinates[1] + itemView.getHeight());
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

    @Override
    public void destroy() {
        for (int i = mPopups.size() - 1; i >= 0; i--) {
            mFlyoutHandler.dismissPopup(mPopups.get(i).popupWindow);
        }
    }

    public void setMainPopupForTest(T popupWindow) {
        mPopups.set(0, new FlyoutPopupEntry(null, popupWindow));
    }
}
