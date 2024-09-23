// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;

import org.chromium.base.ObserverList;
import org.chromium.ui.R;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * A menu button meant to be used with modern lists throughout Chrome. Will automatically show and
 * anchor a popup on press and will rely on a delegate for positioning and content of the popup. You
 * can define your own content description for accessibility through the android:contentDescription
 * parameter in the XML layout of the ListMenuButton. The default content description that
 * corresponds to context.getString(R.string.accessibility_list_menu_button, "") is used otherwise.
 */
public class ListMenuButton extends ChromeImageButton
        implements AnchoredPopupWindow.LayoutObserver {
    /** A listener that is notified when the popup menu is shown or dismissed. */
    @FunctionalInterface
    public interface PopupMenuShownListener {
        void onPopupMenuShown();

        default void onPopupMenuDismissed() {}
    }

    private final boolean mMenuVerticalOverlapAnchor;
    private final boolean mMenuHorizontalOverlapAnchor;

    private int mMenuMaxWidth;
    private AnchoredPopupWindow mPopupMenu;
    private ListMenuButtonDelegate mDelegate;
    private ObserverList<PopupMenuShownListener> mPopupListeners = new ObserverList<>();
    private boolean mTryToFitLargestItem;
    private boolean mPositionedAtEnd;
    private boolean mIsAttachedToWindow;

    /**
     * Creates a new {@link ListMenuButton}.
     *
     * @param context The {@link Context} used to build the visuals from.
     * @param attrs The specific {@link AttributeSet} used to build the button.
     */
    public ListMenuButton(Context context, AttributeSet attrs) {
        super(context, attrs);

        TypedArray a = context.obtainStyledAttributes(attrs, R.styleable.ListMenuButton);
        mMenuMaxWidth =
                a.getDimensionPixelSize(
                        R.styleable.ListMenuButton_menuMaxWidth,
                        getResources().getDimensionPixelSize(R.dimen.list_menu_width));
        mMenuHorizontalOverlapAnchor =
                a.getBoolean(R.styleable.ListMenuButton_menuHorizontalOverlapAnchor, true);
        mMenuVerticalOverlapAnchor =
                a.getBoolean(R.styleable.ListMenuButton_menuVerticalOverlapAnchor, true);
        mPositionedAtEnd = a.getBoolean(R.styleable.ListMenuButton_menuPositionedAtEnd, true);

        a.recycle();
    }

    /**
     * Text that represents the item this menu button is related to.  This will affect the content
     * description of the view {@see #setContentDescription(CharSequence)}.
     *
     * @param context The string representation of the list item this button represents.
     */
    public void setContentDescriptionContext(String context) {
        if (TextUtils.isEmpty(context)) {
            setContentDescription(
                    getContext().getResources().getString(R.string.accessibility_toolbar_btn_menu));
            return;
        }
        setContentDescription(
                getContext()
                        .getResources()
                        .getString(R.string.accessibility_list_menu_button, context));
    }

    /**
     * Sets the delegate this menu will rely on for populating the popup menu and handling selection
     * responses. The OnClickListener will be overridden by default to show menu. The menu will not
     * show or work without the delegate.
     *
     * @param delegate The {@link ListMenuButtonDelegate} to use for menu creation and selection
     *         handling.
     */
    public void setDelegate(ListMenuButtonDelegate delegate) {
        setDelegate(delegate, true);
    }

    /**
     * Sets the delegate this menu will rely on for populating the popup menu and handling selection
     * responses. The menu will not
     * show or work without the delegate.
     *
     * @param delegate The {@link ListMenuButtonDelegate} to use for menu creation and selection
     *         handling.
     * @param overrideOnClickListener Whether to override the click listener which can trigger
     *        the popup menu.
     */
    public void setDelegate(ListMenuButtonDelegate delegate, boolean overrideOnClickListener) {
        dismiss();
        mDelegate = delegate;
        if (overrideOnClickListener) {
            setOnClickListener((view) -> showMenu());
        }
    }

    /** Called to dismiss any popup menu that might be showing for this button. */
    public void dismiss() {
        if (mPopupMenu != null) {
            mPopupMenu.dismiss();
        }
    }

    /** Shows a popupWindow built by ListMenuButton */
    public void showMenu() {
        if (!mIsAttachedToWindow) return;
        dismiss();
        initPopupWindow();
        mPopupMenu.show();
        notifyPopupListeners(true);
    }

    /**
     * Set the max width of the popup menu.
     * @param maxWidth The max width of the popup.
     */
    public void setMenuMaxWidth(int maxWidth) {
        mMenuMaxWidth = maxWidth;
    }

    /** Init the popup window with provided attributes, called before {@link #showMenu()} */
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
                        getContext(),
                        this,
                        new ColorDrawable(Color.TRANSPARENT),
                        contentView,
                        mDelegate.getRectProvider(this));
        mPopupMenu.setVerticalOverlapAnchor(mMenuVerticalOverlapAnchor);
        mPopupMenu.setHorizontalOverlapAnchor(mMenuHorizontalOverlapAnchor);
        mPopupMenu.setMaxWidth(mMenuMaxWidth);
        if (mTryToFitLargestItem) {
            // Content width includes the padding around the items, so add it here.
            final int lateralPadding = contentView.getPaddingLeft() + contentView.getPaddingRight();
            mPopupMenu.setDesiredContentWidth(menu.getMaxItemWidth() + lateralPadding);
        }
        mPopupMenu.setFocusable(true);
        mPopupMenu.setLayoutObserver(this);
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
        if (mPositionedAtEnd) {
            mPopupMenu.setAnimationStyle(
                    positionBelow ? R.style.EndIconMenuAnim : R.style.EndIconMenuAnimBottom);

        } else {
            mPopupMenu.setAnimationStyle(
                    positionBelow ? R.style.StartIconMenuAnim : R.style.StartIconMenuAnimBottom);
        }
    }

    /**
     * Determines whether to try to fit the largest menu item without overflowing by measuring the
     * exact width of each item.
     *
     * WARNING: do not call when the menu list has more than a handful of items, the performance
     * will be terrible since it measures every single item.
     *
     * @param value Determines whether to try to exactly fit the width of the largest item in the
     *              list.
     */
    public void tryToFitLargestItem(boolean value) {
        mTryToFitLargestItem = value;
    }

    // View implementation.
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        if (TextUtils.isEmpty(getContentDescription())) setContentDescriptionContext("");
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        mIsAttachedToWindow = true;
    }

    @Override
    protected void onDetachedFromWindow() {
        dismiss();
        mIsAttachedToWindow = false;
        super.onDetachedFromWindow();
    }

    /**
     * Notify all of the PopupMenuShownListeners of a popup menu action.
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

    public void setAttachedToWindowForTesting() {
        mIsAttachedToWindow = true;
    }
}
