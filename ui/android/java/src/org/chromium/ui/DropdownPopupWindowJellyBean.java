// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.content.Context;
import android.graphics.Rect;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.AdapterView;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ListAdapter;
import android.widget.ListPopupWindow;
import android.widget.ListView;
import android.widget.PopupWindow;

import org.chromium.base.Log;

import java.lang.reflect.Method;

/**
 * The dropdown popup window for Android J. This class is using ListPopupWindow
 * instead of using AnchoredPopupWindow. This class is needed to fix the focus
 * problem that happens on Android J and stops user from selecting dropdown items.
 * (https://crbug.com/836318)
 */
class DropdownPopupWindowJellyBean implements DropdownPopupWindowInterface {
    private static final String TAG = "AutofillPopup";
    private final View mAnchorView;
    private final Context mContext;
    private boolean mRtl;
    private int mInitialSelection = -1;
    private OnLayoutChangeListener mLayoutChangeListener;
    private PopupWindow.OnDismissListener mOnDismissListener;
    private CharSequence mDescription;
    private ListPopupWindow mListPopupWindow;
    private View mFooterView;
    ListAdapter mAdapter;

    /**
     * Creates an DropdownPopupWindowJellyBean with specified parameters.
     * @param context Application context.
     * @param anchorView Popup view to be anchored.
     */
    public DropdownPopupWindowJellyBean(Context context, View anchorView) {
        mListPopupWindow = new ListPopupWindow(context, null, 0, R.style.DropdownPopupWindow);
        mAnchorView = anchorView;

        mAnchorView.setId(R.id.dropdown_popup_window);
        mAnchorView.setTag(this);

        mContext = context;

        mLayoutChangeListener = new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                if (v == mAnchorView) DropdownPopupWindowJellyBean.this.show();
            }
        };
        mAnchorView.addOnLayoutChangeListener(mLayoutChangeListener);

        mListPopupWindow.setOnDismissListener(new PopupWindow.OnDismissListener() {
            @Override
            public void onDismiss() {
                if (mOnDismissListener != null) {
                    mOnDismissListener.onDismiss();
                }
                mAnchorView.removeOnLayoutChangeListener(mLayoutChangeListener);
                mAnchorView.setTag(null);
            }
        });

        mListPopupWindow.setAnchorView(mAnchorView);
        Rect originalPadding = new Rect();
        mListPopupWindow.getBackground().getPadding(originalPadding);
        mListPopupWindow.setVerticalOffset(-originalPadding.top);
    }

    /**
     * Sets the adapter that provides the data and the views to represent the data
     * in this popup window.
     *
     * @param adapter The adapter to use to create this window's content.
     */
    @Override
    public void setAdapter(ListAdapter adapter) {
        mAdapter = adapter;
        mListPopupWindow.setAdapter(adapter);
    }

    @Override
    public void setInitialSelection(int initialSelection) {
        mInitialSelection = initialSelection;
    }

    /**
     * Shows the popup. The adapter should be set before calling this method.
     */
    @Override
    public void show() {
        // An ugly hack to keep the popup from expanding on top of the keyboard.
        mListPopupWindow.setInputMethodMode(ListPopupWindow.INPUT_METHOD_NEEDED);

        assert mAdapter != null : "Set the adapter before showing the popup.";
        final int contentWidth = measureContentWidth();
        final float anchorWidth = mAnchorView.getLayoutParams().width;
        assert anchorWidth > 0;
        Rect padding = new Rect();
        mListPopupWindow.getBackground().getPadding(padding);
        if (contentWidth + padding.left + padding.right > anchorWidth) {
            mListPopupWindow.setContentWidth(contentWidth);
            final Rect displayFrame = new Rect();
            mAnchorView.getWindowVisibleDisplayFrame(displayFrame);
            if (mListPopupWindow.getWidth() > displayFrame.width()) {
                mListPopupWindow.setWidth(displayFrame.width());
            }
        } else {
            mListPopupWindow.setWidth(ViewGroup.LayoutParams.WRAP_CONTENT);
        }
        boolean wasShowing = mListPopupWindow.isShowing();
        mListPopupWindow.show();
        mListPopupWindow.getListView().setDividerHeight(0);
        int layoutDirection = mRtl ? View.LAYOUT_DIRECTION_RTL : View.LAYOUT_DIRECTION_LTR;
        mListPopupWindow.getListView().setLayoutDirection(layoutDirection);
        if (!wasShowing) {
            mListPopupWindow.getListView().setContentDescription(mDescription);
            mListPopupWindow.getListView().sendAccessibilityEvent(
                    AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED);
        }
        if (mInitialSelection >= 0) {
            mListPopupWindow.getListView().setSelection(mInitialSelection);
            mInitialSelection = -1;
        }
    }

    /**
     * Set a listener to receive a callback when the popup is dismissed.
     *
     * @param listener Listener that will be notified when the popup is dismissed.
     */
    @Override
    public void setOnDismissListener(PopupWindow.OnDismissListener listener) {
        mOnDismissListener = listener;
    }

    /**
     * Sets the text direction in the dropdown. Should be called before show().
     * @param isRtl If true, then dropdown text direction is right to left.
     */
    @Override
    public void setRtl(boolean isRtl) {
        mRtl = isRtl;
    }

    /**
     * Disable hiding on outside tap so that tapping on a text input field associated with the popup
     * will not hide the popup.
     */
    @Override
    public void disableHideOnOutsideTap() {
        // HACK: The ListPopupWindow's mPopup automatically dismisses on an outside tap. There's
        // no way to override it or prevent it, except reaching into ListPopupWindow's hidden
        // API. This allows the C++ controller to completely control showing/hiding the popup.
        // See http://crbug.com/400601
        try {
            Method setForceIgnoreOutsideTouch = ListPopupWindow.class.getMethod(
                    "setForceIgnoreOutsideTouch", new Class[] {boolean.class});
            setForceIgnoreOutsideTouch.invoke(mListPopupWindow, new Object[] {true});
        } catch (Exception e) {
            Log.e(TAG, "ListPopupWindow.setForceIgnoreOutsideTouch not found", e);
        }
    }

    /**
     * Sets the content description to be announced by accessibility services when the dropdown is
     * shown.
     * @param description The description of the content to be announced.
     */
    @Override
    public void setContentDescriptionForAccessibility(CharSequence description) {
        mDescription = description;
    }

    /**
     * Sets a listener to receive events when a list item is clicked.
     *
     * @param clickListener Listener to register
     */
    @Override
    public void setOnItemClickListener(AdapterView.OnItemClickListener clickListener) {
        mListPopupWindow.setOnItemClickListener(clickListener);
    }

    @Override
    public void setFooterView(View footerView) {
        mListPopupWindow.setPromptPosition(ListPopupWindow.POSITION_PROMPT_BELOW);
        if (footerView != null) {
            footerView.setLayoutParams(new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
            mFooterView = LayoutInflater.from(mContext).inflate(
                    R.layout.dropdown_footer_wrapper_jellybean, null);
            FrameLayout container = (FrameLayout) mFooterView.findViewById(R.id.dropdown_footer);
            container.addView(footerView);
        } else {
            mFooterView = null;
        }
        mListPopupWindow.setPromptView(mFooterView);
    }

    /**
     * Show the popup. Will have no effect if the popup is already showing.
     * Post a {@link #show()} call to the UI thread.
     */
    @Override
    public void postShow() {
        mListPopupWindow.postShow();
    }

    /**
     * Disposes of the popup window.
     */
    @Override
    public void dismiss() {
        mListPopupWindow.dismiss();
    }

    /**
     * @return The {@link ListView} displayed within the popup window.
     */
    @Override
    public ListView getListView() {
        return mListPopupWindow.getListView();
    }

    /**
     * @return Whether the popup is currently showing.
     */
    @Override
    public boolean isShowing() {
        return mListPopupWindow.isShowing();
    }

    /**
     * Measures the width of the list content. The adapter should not be null.
     * @return The popup window width in pixels.
     */
    private int measureContentWidth() {
        assert mAdapter != null : "Set the adapter before showing the popup.";
        int adapterWidth = UiUtils.computeMaxWidthOfListAdapterItems(mAdapter);
        if (mFooterView != null) {
            if (mFooterView.getLayoutParams() == null) {
                mFooterView.setLayoutParams(new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
            }
            int measureSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
            mFooterView.measure(measureSpec, measureSpec);
            return Math.max(mFooterView.getMeasuredWidth(), adapterWidth);
        }
        return adapterWidth;
    }
}