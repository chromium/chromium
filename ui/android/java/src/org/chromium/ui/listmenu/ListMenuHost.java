// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.res.TypedArray;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;

import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.R;
import org.chromium.ui.widget.AnchoredPopupWindow;

/**
 * The host class that makes a view capable of triggering list menu. The core logic is extracted
 * from ListMenuButton.
 */
@NullMarked
public class ListMenuHost implements AnchoredPopupWindow.LayoutObserver {
    /** A listener that is notified when the popup menu is shown or dismissed. */
    @FunctionalInterface
    public interface PopupMenuShownListener {
        void onPopupMenuShown();

        default void onPopupMenuDismissed() {}
    }

    @FunctionalInterface
    interface PopupMenuHelper {
        /**
         * Called when the popup menu is requested to be shown or dismissed, then inject a test
         * value.
         *
         * @param menu The popup menu owned by the ListMenuHost.
         * @return The menu to be owned during test.
         */
        AnchoredPopupWindow injectPopupMenu(@Nullable AnchoredPopupWindow menu);
    }

    private static ListMenuHost.@Nullable PopupMenuHelper sPopupMenuHelperForTesting;

    private final View mView;
    private final boolean mMenuVerticalOverlapAnchor;
    private final boolean mMenuHorizontalOverlapAnchor;

    private int mMenuMaxWidth;

    private @Nullable AnchoredPopupWindow mPopupMenu;
    private @Nullable ListMenuDelegate mDelegate;
    private final ObserverList<PopupMenuShownListener> mPopupListeners = new ObserverList<>();
    private boolean mTryToFitLargestItem;
    private final boolean mPositionedAtStart;
    private final boolean mPositionedAtEnd;

    /**
     * Creates a new {@link ListMenuHost}.
     *
     * @param view The {@link View} used to trigger list menu.
     * @param attrs The specific {@link AttributeSet} used to set read styles.
     */
    public ListMenuHost(View view, @Nullable AttributeSet attrs) {
        mView = view;

        TypedArray a = view.getContext().obtainStyledAttributes(attrs, R.styleable.ListMenuButton);
        mMenuMaxWidth =
                a.getDimensionPixelSize(
                        R.styleable.ListMenuButton_menuMaxWidth,
                        mView.getResources().getDimensionPixelSize(R.dimen.list_menu_width));
        mMenuHorizontalOverlapAnchor =
                a.getBoolean(R.styleable.ListMenuButton_menuHorizontalOverlapAnchor, true);
        mMenuVerticalOverlapAnchor =
                a.getBoolean(R.styleable.ListMenuButton_menuVerticalOverlapAnchor, true);
        mPositionedAtStart = a.getBoolean(R.styleable.ListMenuButton_menuPositionedAtStart, false);
        mPositionedAtEnd = a.getBoolean(R.styleable.ListMenuButton_menuPositionedAtEnd, false);

        assert !(mPositionedAtStart && mPositionedAtEnd)
                : "menuPositionedAtStart and menuPositionedAtEnd are both true.";

        a.recycle();
    }

    /**
     * Sets the delegate this menu will rely on for populating the popup menu and handling selection
     * responses. The menu will not show or work without the delegate.
     *
     * @param delegate The {@link ListMenuDelegate} to use for menu creation and selection handling.
     * @param overrideOnClickListener Whether to override the click listener which can trigger the
     *     popup menu.
     */
    public void setDelegate(@Nullable ListMenuDelegate delegate, boolean overrideOnClickListener) {
        dismiss();
        mDelegate = delegate;
        if (overrideOnClickListener) {
            mView.setOnClickListener((view) -> showMenu());
        }
    }

    /** Called to dismiss any popup menu that might be showing for this button. */
    public void dismiss() {
        if (mPopupMenu != null) {
            mPopupMenu.dismiss();
            mPopupMenu = null;

            if (sPopupMenuHelperForTesting != null) {
                mPopupMenu = sPopupMenuHelperForTesting.injectPopupMenu(null);
            }
        }
    }

    /** Shows a popupWindow built by ListMenuButton */
    public void showMenu() {
        if (!mView.isAttachedToWindow()) return;
        dismiss();
        initPopupWindow();
        mPopupMenu.show();
        notifyPopupListeners(true);
    }

    /**
     * Set the max width of the popup menu.
     *
     * @param maxWidth The max width of the popup.
     */
    public void setMenuMaxWidth(int maxWidth) {
        mMenuMaxWidth = maxWidth;
    }

    /** Init the popup window with provided attributes, called before {@link #showMenu()} */
    @EnsuresNonNull("mPopupMenu")
    private void initPopupWindow() {
        if (mDelegate == null) throw new IllegalStateException("Delegate was not set.");

        ListMenu menu = mDelegate.getListMenu();
        menu.addContentViewClickRunnable(this::dismiss);

        final View contentView = menu.getContentView();
        ViewParent viewParent = contentView.getParent();
        // TODO(crbug.com/40838478): figure out why contentView is not removed from popup menu.
        if (viewParent instanceof ViewGroup) {
            ((ViewGroup) viewParent).removeView(contentView);
        }
        mPopupMenu =
                new AnchoredPopupWindow(
                        mView.getContext(),
                        mView,
                        new ColorDrawable(Color.TRANSPARENT),
                        contentView,
                        mDelegate.getRectProvider(mView));

        if (sPopupMenuHelperForTesting != null) {
            mPopupMenu = sPopupMenuHelperForTesting.injectPopupMenu(mPopupMenu);
        }

        mPopupMenu.setVerticalOverlapAnchor(mMenuVerticalOverlapAnchor);
        mPopupMenu.setHorizontalOverlapAnchor(mMenuHorizontalOverlapAnchor);
        mPopupMenu.setMaxWidth(mMenuMaxWidth);
        if (mTryToFitLargestItem) {
            // Content width includes the padding around the items, so add it here.
            final int lateralPadding = contentView.getPaddingLeft() + contentView.getPaddingRight();
            mPopupMenu.setDesiredContentWidth(menu.getMaxItemWidth() + lateralPadding);
        }
        mPopupMenu.setFocusable(true);
        mPopupMenu.setAnimateFromAnchor(true);
        if (mPositionedAtStart || mPositionedAtEnd) {
            mPopupMenu.setLayoutObserver(this);
        }
        mPopupMenu.addOnDismissListener(
                () -> {
                    mPopupMenu = null;
                    notifyPopupListeners(false);
                });
        // This should be called explicitly since it is not a default behavior on Android S
        // in split-screen mode. See crbug.com/1246956.
        mPopupMenu.setOutsideTouchable(true);
    }

    /**
     * Adds a listener which will be notified when the popup menu is shown.
     *
     * @param l The listener of interest.
     */
    public void addPopupListener(PopupMenuShownListener l) {
        mPopupListeners.addObserver(l);
    }

    /**
     * Removes a popup menu listener.
     *
     * @param l The listener of interest.
     */
    public void removePopupListener(PopupMenuShownListener l) {
        mPopupListeners.removeObserver(l);
    }

    // AnchoredPopupWindow.LayoutObserver implementation.
    @Override
    public void onPreLayoutChange(
            boolean positionBelow, int x, int y, int width, int height, Rect anchorRect) {
        assumeNonNull(mPopupMenu);
        if (mPositionedAtEnd) {
            mPopupMenu.setAnimationStyle(
                    positionBelow ? R.style.EndIconMenuAnim : R.style.EndIconMenuAnimBottom);
        } else if (mPositionedAtStart) {
            mPopupMenu.setAnimationStyle(
                    positionBelow ? R.style.StartIconMenuAnim : R.style.StartIconMenuAnimBottom);
        }
    }

    /**
     * Determines whether to try to fit the largest menu item without overflowing by measuring the
     * exact width of each item.
     *
     * <p>WARNING: do not call when the menu list has more than a handful of items, the performance
     * will be terrible since it measures every single item.
     *
     * @param value Determines whether to try to exactly fit the width of the largest item in the
     *     list.
     */
    public void tryToFitLargestItem(boolean value) {
        mTryToFitLargestItem = value;
    }

    /**
     * Notify all of the PopupMenuShownListeners of a popup menu action.
     *
     * @param shown Whether the popup menu was shown or dismissed.
     */
    private void notifyPopupListeners(boolean shown) {
        for (var l : mPopupListeners.mObservers) {
            if (shown) {
                l.onPopupMenuShown();
            } else {
                l.onPopupMenuDismissed();
            }
        }
    }

    static void setMenuChangedListenerForTesting(PopupMenuHelper listener) {
        sPopupMenuHelperForTesting = listener;
        ResettersForTesting.register(() -> sPopupMenuHelperForTesting = null);
    }
}
