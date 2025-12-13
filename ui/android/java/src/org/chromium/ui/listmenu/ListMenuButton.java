// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import android.content.Context;
import android.os.Build;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.R;
import org.chromium.ui.util.MotionEventUtils;
import org.chromium.ui.widget.ChromeImageButton;

/**
 * A menu button meant to be used with modern lists throughout Chrome. Will automatically show and
 * anchor a popup on press and will rely on a delegate for positioning and content of the popup. You
 * can define your own content description for accessibility through the android:contentDescription
 * parameter in the XML layout of the ListMenuButton. The default content description that
 * corresponds to context.getString(R.string.accessibility_list_menu_button, "") is used otherwise.
 */
@NullMarked
public class ListMenuButton extends ChromeImageButton {
    private final ListMenuHost mListMenuHost;
    private boolean mIsAttachedToWindow;

    /**
     * Creates a new {@link ListMenuButton}.
     *
     * @param context The {@link Context} used to build the visuals from.
     * @param attrs The specific {@link AttributeSet} used to build the button.
     */
    public ListMenuButton(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
        mListMenuHost = new ListMenuHost(this, attrs);
    }

    /**
     * Text that represents the item this menu button is related to. This will affect the content
     * description of the view {@see #setContentDescription(CharSequence)}.
     *
     * @param context The string representation of the list item this button represents.
     */
    public void setContentDescriptionContext(String context) {
        if (TextUtils.isEmpty(context)) {
            setContentDescription(getContext().getString(R.string.accessibility_toolbar_btn_menu));
            return;
        }
        setContentDescription(
                getContext().getString(R.string.accessibility_list_menu_button, context));
    }

    /**
     * Sets the delegate this menu will rely on for populating the popup menu and handling selection
     * responses. The OnClickListener will be overridden by default to show menu. The menu will not
     * show or work without the delegate.
     *
     * @param delegate The {@link ListMenuDelegate} to use for menu creation and selection handling.
     */
    public void setDelegate(@Nullable ListMenuDelegate delegate) {
        setDelegate(delegate, true);
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
        mListMenuHost.setDelegate(delegate, overrideOnClickListener);
    }

    /**
     * Set the root view for {@link AnchoredPopupWindow} to use. This is necessary when the root
     * view of {@link mView} does not match the root view of the application, for example when the
     * {@link mView} is inside another {@link AnchoredPopupWindow}. This must be called before the
     * popup window is shown.
     *
     * @param rootView The {@link View} to use to get window tokens.
     */
    public void setRootView(View rootView) {
        mListMenuHost.setRootView(rootView);
    }

    /**
     * @returns The {@link ListMenuHost} of the menu.
     */
    public ListMenuHost getHost() {
        return mListMenuHost;
    }

    /** Called to dismiss any popup menu that might be showing for this button. */
    public void dismiss() {
        mListMenuHost.dismiss();
    }

    /** Shows a popupWindow built by ListMenuButton */
    public void showMenu() {
        if (!mIsAttachedToWindow) return;
        mListMenuHost.showMenu();
    }

    /**
     * Set the max width of the popup menu.
     *
     * @param maxWidth The max width of the popup.
     */
    public void setMenuMaxWidth(int maxWidth) {
        mListMenuHost.setMenuMaxWidth(maxWidth);
    }

    /**
     * Adds a listener which will be notified when the popup menu is shown.
     *
     * @param l The listener of interest.
     */
    public void addPopupListener(ListMenuHost.PopupMenuShownListener l) {
        mListMenuHost.addPopupListener(l);
    }

    /**
     * Removes a popup menu listener.
     *
     * @param l The listener of interest.
     */
    public void removePopupListener(ListMenuHost.PopupMenuShownListener l) {
        mListMenuHost.removePopupListener(l);
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
        mListMenuHost.tryToFitLargestItem(value);
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

    @Override
    public boolean onGenericMotionEvent(MotionEvent event) {
        // Treat secondary clicks as long clicks.
        if (MotionEventUtils.isSecondaryClick(event.getButtonState())
                && Build.VERSION.SDK_INT >= Build.VERSION_CODES.R
                && hasOnLongClickListeners()) {
            return performLongClick();
        }
        return super.onGenericMotionEvent(event);
    }

    public void setAttachedToWindowForTesting() {
        mIsAttachedToWindow = true;
    }
}
