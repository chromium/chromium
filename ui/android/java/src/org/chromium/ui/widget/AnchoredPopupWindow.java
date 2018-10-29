// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.support.annotation.IntDef;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.View.OnTouchListener;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.PopupWindow;
import android.widget.PopupWindow.OnDismissListener;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.VisibleForTesting;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * UI component that handles showing a {@link PopupWindow}. Positioning this popup happens through
 * a {@link RectProvider} provided during construction.
 */
public class AnchoredPopupWindow implements OnTouchListener, RectProvider.Observer {
    /**
     * An observer that is notified of AnchoredPopupWindow layout changes.
     */
    public interface LayoutObserver {
        /**
         * Called immediately before the popup layout changes.
         * @param positionBelow Whether the popup is positioned below its anchor rect.
         * @param x The x position for the popup.
         * @param y The y position for the popup.
         * @param width The width of the popup.
         * @param height The height of the popup.
         * @param anchorRect The {@link Rect} used to anchor the popup.
         */
        void onPreLayoutChange(
                boolean positionBelow, int x, int y, int width, int height, Rect anchorRect);
    }

    /** VerticalOrientation preferences for the popup */
    @IntDef({VERTICAL_ORIENTATION_MAX_AVAILABLE_SPACE, VERTICAL_ORIENTATION_BELOW,
            VERTICAL_ORIENTATION_ABOVE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface VerticalOrientation {}
    /**
     * Vertically position to whichever side of the anchor has more available space. The popup
     * will be sized to ensure it fits on screen.
     */
    public static final int VERTICAL_ORIENTATION_MAX_AVAILABLE_SPACE = 0;
    /** Position below the anchor if there is enough space. */
    public static final int VERTICAL_ORIENTATION_BELOW = 1;
    /** Position above the anchor if there is enough space. */
    public static final int VERTICAL_ORIENTATION_ABOVE = 2;

    /** HorizontalOrientation preferences for the popup */
    @IntDef({HORIZONTAL_ORIENTATION_MAX_AVAILABLE_SPACE, HORIZONTAL_ORIENTATION_CENTER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface HorizontalOrientation {}
    /**
     * Horizontally position to whichever side of the anchor has more available space. The popup
     * will be sized to ensure it fits on screen.
     */
    public static final int HORIZONTAL_ORIENTATION_MAX_AVAILABLE_SPACE = 0;
    /**
     * Horizontally center with respect to the anchor, so long as the popup still fits on the
     * screen.
     */
    public static final int HORIZONTAL_ORIENTATION_CENTER = 1;

    // Cache Rect objects for querying View and Screen coordinate APIs.
    private final Rect mCachedPaddingRect = new Rect();
    private final Rect mCachedWindowRect = new Rect();

    private final Context mContext;
    private final Handler mHandler;
    private final View mRootView;

    /** The actual {@link PopupWindow}.  Internalized to prevent API leakage. */
    private final PopupWindow mPopupWindow;

    /** Provides the @link Rect} to anchor the popup to in screen space. */
    private final RectProvider mRectProvider;

    private final Runnable mDismissRunnable = new Runnable() {
        @Override
        public void run() {
            if (mPopupWindow.isShowing()) dismiss();
        }
    };

    private final OnDismissListener mDismissListener = new OnDismissListener() {
        @Override
        public void onDismiss() {
            if (mIgnoreDismissal) return;

            mHandler.removeCallbacks(mDismissRunnable);
            for (OnDismissListener listener : mDismissListeners) listener.onDismiss();

            mRectProvider.stopObserving();
        }
    };

    private boolean mDismissOnTouchInteraction;

    // Pass through for the internal PopupWindow.  This class needs to intercept these for API
    // purposes, but they are still useful to callers.
    private ObserverList<OnDismissListener> mDismissListeners = new ObserverList<>();
    private OnTouchListener mTouchListener;
    private LayoutObserver mLayoutObserver;

    // Positioning/sizing coordinates for the popup.
    private int mX;
    private int mY;
    private int mWidth;
    private int mHeight;

    /** The margin to add to the popup so it doesn't bump against the edges of the screen. */
    private int mMarginPx;

    /**
     * The maximum width of the popup. This width is used as long as the popup still fits on screen.
     */
    private int mMaxWidthPx;

    // Preferred orientation for the popup with respect to the anchor.
    // Preferred vertical orientation for the popup with respect to the anchor.
    @VerticalOrientation
    private int mPreferredVerticalOrientation = VERTICAL_ORIENTATION_MAX_AVAILABLE_SPACE;
    // Preferred horizontal orientation for the popup with respect to the anchor.
    @HorizontalOrientation
    private int mPreferredHorizontalOrientation = HORIZONTAL_ORIENTATION_MAX_AVAILABLE_SPACE;

    /**
     * Tracks whether or not we are in the process of updating the popup, which might include a
     * dismiss and show.  In that case we don't want to let the world know we're dismissing because
     * it's only temporary.
     */
    private boolean mIgnoreDismissal;

    private boolean mPositionBelow;
    private boolean mPositionToLeft;
    private boolean mVerticalOverlapAnchor;
    private boolean mHorizontalOverlapAnchor;
    private boolean mUpdateOrientationOnChange;

    /**
     * Constructs an {@link AnchoredPopupWindow} instance.
     * @param context  Context to draw resources from.
     * @param rootView The {@link View} to use for size calculations and for display.
     * @param background The background {@link Drawable} to use for the popup.
     * @param contentView The content view to set on the popup.
     * @param anchorRectProvider The {@link RectProvider} that will provide the {@link Rect} this
     *                           popup attaches and orients to.
     */
    public AnchoredPopupWindow(Context context, View rootView, Drawable background,
            View contentView, RectProvider anchorRectProvider) {
        mContext = context;
        mRootView = rootView.getRootView();
        mPopupWindow = UiWidgetFactory.getInstance().createPopupWindow(mContext);
        mHandler = new Handler();
        mRectProvider = anchorRectProvider;

        mPopupWindow.setWidth(ViewGroup.LayoutParams.WRAP_CONTENT);
        mPopupWindow.setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);
        mPopupWindow.setBackgroundDrawable(background);
        mPopupWindow.setContentView(contentView);

        mPopupWindow.setTouchInterceptor(this);
        mPopupWindow.setOnDismissListener(mDismissListener);
    }

    /** Shows the popup. Will have no effect if the popup is already showing. */
    public void show() {
        if (mPopupWindow.isShowing()) return;

        mRectProvider.startObserving(this);

        updatePopupLayout();
        showPopupWindow();
    }

    /**
     * Disposes of the popup window.  Will have no effect if the popup isn't showing.
     * @see PopupWindow#dismiss()
     */
    public void dismiss() {
        mPopupWindow.dismiss();
    }

    /**
     * @return Whether the popup is currently showing.
     */
    public boolean isShowing() {
        return mPopupWindow.isShowing();
    }

    /**
     * Sets the {@link LayoutObserver} for this AnchoredPopupWindow.
     * @param layoutObserver The observer to be notified of layout changes.
     */
    public void setLayoutObserver(LayoutObserver layoutObserver) {
        mLayoutObserver = layoutObserver;
    }

    /**
     * @param onTouchListener A callback for all touch events being dispatched to the popup.
     * @see PopupWindow#setTouchInterceptor(OnTouchListener)
     */
    public void setTouchInterceptor(OnTouchListener onTouchListener) {
        mTouchListener = onTouchListener;
    }

    /**
     * @param onDismissListener A listener to be called when the popup is dismissed.
     * @see PopupWindow#setOnDismissListener(OnDismissListener)
     */
    public void addOnDismissListener(OnDismissListener onDismissListener) {
        mDismissListeners.addObserver(onDismissListener);
    }

    /**
     * @param onDismissListener The listener to remove and not call when the popup is dismissed.
     * @see PopupWindow#setOnDismissListener(OnDismissListener)
     */
    public void removeOnDismissListener(OnDismissListener onDismissListener) {
        mDismissListeners.removeObserver(onDismissListener);
    }

    /**
     * @param dismiss Whether or not to dismiss this popup when the screen is tapped.  This will
     *                happen for both taps inside and outside the popup.  The default is
     *                {@code false}.
     */
    public void setDismissOnTouchInteraction(boolean dismiss) {
        mDismissOnTouchInteraction = dismiss;
        mPopupWindow.setOutsideTouchable(mDismissOnTouchInteraction);
    }

    /**
     * If set to true, popup will be notified when an outside touch happens.
     * It is not the equivalent of closing the popup on all touch events. The user can
     * still interact with the popup by sending inside touch events.
     * If set to false, the popup won't be notified about the outside touch event.
     *
     * @param touchable Whether or not to notify the popup when an outside touch
     *                  happens. The default is {@code false}.
     */
    public void setOutsideTouchable(boolean touchable) {
        mPopupWindow.setOutsideTouchable(touchable);
    }

    /**
     * Sets the preferred vertical orientation of the popup with respect to the anchor Rect such as
     * above or below the anchor.  This should be called before the popup is shown.
     * @param orientation The vertical orientation preferred.
     */
    public void setPreferredVerticalOrientation(@VerticalOrientation int orientation) {
        mPreferredVerticalOrientation = orientation;
    }

    /**
     * Sets the preferred horizontal orientation of the popup with respect to the anchor Rect such
     * as centered with respect to the anchor.  This should be called before the popup is shown.
     * @param orientation The horizontal orientation preferred.
     */
    public void setPreferredHorizontalOrientation(@HorizontalOrientation int orientation) {
        mPreferredHorizontalOrientation = orientation;
    }

    /**
     * Sets the animation style for the popup.  This should be called before the popup is shown.
     * @param animationStyleId The id of the animation style.
     */
    public void setAnimationStyle(int animationStyleId) {
        mPopupWindow.setAnimationStyle(animationStyleId);
    }

    /**
     * If set to true, orientation will be updated everytime that the {@link OnRectChanged} is
     * called.
     */
    public void setUpdateOrientationOnChange(boolean updateOrientationOnChange) {
        mUpdateOrientationOnChange = updateOrientationOnChange;
    }

    /**
     * Changes the focusability of the popup. See {@link PopupWindow#setFocusable(boolean)}.
     * @param focusable True if the popup is focusable, false otherwise.
     */
    public void setFocusable(boolean focusable) {
        mPopupWindow.setFocusable(focusable);
    }

    /**
     * Sets the margin for the popup window.  This should be called before the popup is shown.
     * @param margin The margin in pixels.
     */
    public void setMargin(int margin) {
        mMarginPx = margin;
    }

    /**
     * Sets the max width for the popup.  This should be called before the popup is shown.
     * @param maxWidth The max width for the popup.
     */
    public void setMaxWidth(int maxWidth) {
        mMaxWidthPx = maxWidth;
    }

    /**
     * Sets whether the popup should horizontally overlap the anchor {@link Rect}.
     * Defaults to false.  This should be called before the popup is shown.
     * @param overlap Whether the popup should overlap the anchor.
     */
    public void setHorizontalOverlapAnchor(boolean overlap) {
        mHorizontalOverlapAnchor = overlap;
    }

    /**
     * Sets whether the popup should vertically overlap the anchor {@link Rect}.
     * Defaults to false.  This should be called before the popup is shown.
     * @param overlap Whether the popup should overlap the anchor.
     */
    public void setVerticalOverlapAnchor(boolean overlap) {
        mVerticalOverlapAnchor = overlap;
    }

    /**
     * Changes the background of the popup.
     * @param background The {@link Drawable} that is set to be background.
     */
    public void setBackgroundDrawable(Drawable background) {
        mPopupWindow.setBackgroundDrawable(background);
    }

    /**
     * Sets the elevation of the popup, if elevation is supported.
     */
    public void setElevation(float elevation) {
        ApiCompatibilityUtils.setElevation(mPopupWindow, elevation);
    }

    // RectProvider.Observer implementation.
    @Override
    public void onRectChanged() {
        updatePopupLayout();
    }

    @Override
    public void onRectHidden() {
        dismiss();
    }

    /**
     * Causes this popup to position/size itself.  The calculations will happen even if the popup
     * isn't visible.
     */
    private void updatePopupLayout() {
        // TODO(twellington): Add more unit tests for this large method.

        // Determine the size of the text popup.
        boolean currentPositionBelow = mPositionBelow;
        boolean currentPositionToLeft = mPositionToLeft;
        boolean preferCurrentOrientation = mPopupWindow.isShowing() && !mUpdateOrientationOnChange;

        mPopupWindow.getBackground().getPadding(mCachedPaddingRect);
        int paddingX = mCachedPaddingRect.left + mCachedPaddingRect.right;
        int paddingY = mCachedPaddingRect.top + mCachedPaddingRect.bottom;

        int maxContentWidth =
                getMaxContentWidth(mMaxWidthPx, mRootView.getWidth(), mMarginPx, paddingX);

        // Determine whether or not the popup should be above or below the anchor.
        // Aggressively try to put it below the anchor.  Put it above only if it would fit better.
        View contentView = mPopupWindow.getContentView();
        int widthSpec = MeasureSpec.makeMeasureSpec(maxContentWidth, MeasureSpec.AT_MOST);
        contentView.measure(widthSpec, MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        int idealContentHeight = contentView.getMeasuredHeight();
        int idealContentWidth = contentView.getMeasuredWidth();

        mRootView.getWindowVisibleDisplayFrame(mCachedWindowRect);

        // In multi-window, the coordinates of root view will be different than (0,0).
        // So we translate the coordinates of |mCachedWindowRect| w.r.t. its window.
        int[] rootCoordinates = new int[2];
        mRootView.getLocationOnScreen(rootCoordinates);
        mCachedWindowRect.offset(-rootCoordinates[0], -rootCoordinates[1]);

        Rect anchorRect = mRectProvider.getRect();
        // TODO(dtrainor): This follows the previous logic.  But we should look into if we want to
        // use the root view dimensions instead of the window dimensions here so the popup can't
        // bleed onto the decorations.
        int spaceAboveAnchor = (mVerticalOverlapAnchor ? anchorRect.bottom : anchorRect.top)
                - mCachedWindowRect.top - paddingY - mMarginPx;
        int spaceBelowAnchor = mCachedWindowRect.bottom
                - (mVerticalOverlapAnchor ? anchorRect.top : anchorRect.bottom) - paddingY
                - mMarginPx;

        // Bias based on the center of the popup and where it is on the screen.
        boolean idealFitsBelow = idealContentHeight <= spaceBelowAnchor;
        boolean idealFitsAbove = idealContentHeight <= spaceAboveAnchor;

        // Position the popup in the largest available space where it can fit.  This will bias the
        // popups to show below the anchor if it will not fit in either place.
        mPositionBelow =
                (idealFitsBelow && spaceBelowAnchor >= spaceAboveAnchor) || !idealFitsAbove;

        // Override the ideal popup orientation if we are trying to maintain the current one.
        if (preferCurrentOrientation && currentPositionBelow != mPositionBelow) {
            if (currentPositionBelow && idealFitsBelow) mPositionBelow = true;
            if (!currentPositionBelow && idealFitsAbove) mPositionBelow = false;
        }

        if (mPreferredVerticalOrientation == VERTICAL_ORIENTATION_BELOW && idealFitsBelow) {
            mPositionBelow = true;
        }
        if (mPreferredVerticalOrientation == VERTICAL_ORIENTATION_ABOVE && idealFitsAbove) {
            mPositionBelow = false;
        }

        if (mPreferredHorizontalOrientation == HORIZONTAL_ORIENTATION_MAX_AVAILABLE_SPACE) {
            int spaceLeftOfAnchor =
                    getSpaceLeftOfAnchor(anchorRect, mCachedWindowRect, mHorizontalOverlapAnchor);
            int spaceRightOfAnchor =
                    getSpaceRightOfAnchor(anchorRect, mCachedWindowRect, mHorizontalOverlapAnchor);
            mPositionToLeft = shouldPositionLeftOfAnchor(spaceLeftOfAnchor, spaceRightOfAnchor,
                    idealContentWidth + paddingY + mMarginPx, currentPositionToLeft,
                    preferCurrentOrientation);

            // TODO(twellington): Update popup width if the ideal width is greater than the space
            // to the left or right of the anchor.
        }

        int maxContentHeight = mPositionBelow ? spaceBelowAnchor : spaceAboveAnchor;
        contentView.measure(
                widthSpec, MeasureSpec.makeMeasureSpec(maxContentHeight, MeasureSpec.AT_MOST));

        mWidth = contentView.getMeasuredWidth() + paddingX;
        mHeight = contentView.getMeasuredHeight() + paddingY;

        // Determine the position of the text popup.
        mX = getPopupX(anchorRect, mCachedWindowRect, mWidth, mMarginPx, mHorizontalOverlapAnchor,
                mPreferredHorizontalOrientation, mPositionToLeft);
        mY = getPopupY(anchorRect, mHeight, mVerticalOverlapAnchor, mPositionBelow);

        if (mLayoutObserver != null) {
            mLayoutObserver.onPreLayoutChange(mPositionBelow, mX, mY, mWidth, mHeight, anchorRect);
        }

        if (mPopupWindow.isShowing() && mPositionBelow != currentPositionBelow) {
            // If the position of the popup has changed, callers may change the background drawable
            // in response. In this case the padding of the background drawable in the PopupWindow
            // changes.
            try {
                mIgnoreDismissal = true;
                mPopupWindow.dismiss();
                showPopupWindow();
            } finally {
                mIgnoreDismissal = false;
            }
        }

        mPopupWindow.update(mX, mY, mWidth, mHeight);
    }

    @VisibleForTesting
    static int getMaxContentWidth(
            int desiredMaxWidthPx, int rootViewWidth, int marginPx, int paddingX) {
        int maxWidthBasedOnRootView = rootViewWidth - marginPx * 2;
        int maxWidth;
        if (desiredMaxWidthPx != 0 && desiredMaxWidthPx < maxWidthBasedOnRootView) {
            maxWidth = desiredMaxWidthPx;
        } else {
            maxWidth = maxWidthBasedOnRootView;
        }

        return maxWidth > paddingX ? maxWidth - paddingX : 0;
    }

    @VisibleForTesting
    static int getSpaceLeftOfAnchor(Rect anchorRect, Rect windowRect, boolean overlapAnchor) {
        return (overlapAnchor ? anchorRect.right : anchorRect.left) - windowRect.left;
    }

    @VisibleForTesting
    static int getSpaceRightOfAnchor(Rect anchorRect, Rect windowRect, boolean overlapAnchor) {
        return windowRect.right - (overlapAnchor ? anchorRect.left : anchorRect.right);
    }

    @VisibleForTesting
    static boolean shouldPositionLeftOfAnchor(int spaceToLeftOfAnchor, int spaceToRightOfAnchor,
            int idealPopupWidth, boolean currentPositionToLeft, boolean preferCurrentOrientation) {
        boolean positionToLeft = spaceToLeftOfAnchor >= spaceToRightOfAnchor;

        // Override the ideal popup orientation if we are trying to maintain the current one.
        if (preferCurrentOrientation && positionToLeft != currentPositionToLeft) {
            if (currentPositionToLeft && idealPopupWidth <= spaceToLeftOfAnchor) {
                positionToLeft = true;
            }
            if (!currentPositionToLeft && idealPopupWidth <= spaceToRightOfAnchor) {
                positionToLeft = false;
            }
        }

        return positionToLeft;
    }

    @VisibleForTesting
    static int getPopupX(Rect anchorRect, Rect windowRect, int popupWidth, int marginPx,
            boolean overlapAnchor, @HorizontalOrientation int horizontalOrientation,
            boolean positionToLeft) {
        int x;

        if (horizontalOrientation == HORIZONTAL_ORIENTATION_CENTER) {
            x = anchorRect.left + (anchorRect.width() - popupWidth) / 2 + marginPx;
        } else if (positionToLeft) {
            x = (overlapAnchor ? anchorRect.right : anchorRect.left) - popupWidth;
        } else {
            x = overlapAnchor ? anchorRect.left : anchorRect.right;
        }

        // In landscape mode, root view includes the decorations in some devices. So we guard the
        // window dimensions against |windowRect.right| instead.
        return clamp(x, marginPx, windowRect.right - popupWidth - marginPx);
    }

    @VisibleForTesting
    static int getPopupY(
            Rect anchorRect, int popupHeight, boolean overlapAnchor, boolean positionBelow) {
        if (positionBelow) {
            return overlapAnchor ? anchorRect.top : anchorRect.bottom;
        } else {
            return (overlapAnchor ? anchorRect.bottom : anchorRect.top) - popupHeight;
        }
    }

    // OnTouchListener implementation.
    @SuppressLint("ClickableViewAccessibility")
    @Override
    public boolean onTouch(View v, MotionEvent event) {
        boolean returnValue = mTouchListener != null && mTouchListener.onTouch(v, event);
        if (mDismissOnTouchInteraction) dismiss();
        return returnValue;
    }

    private static int clamp(int value, int a, int b) {
        int min = (a > b) ? b : a;
        int max = (a > b) ? a : b;
        if (value < min) {
            value = min;
        } else if (value > max) {
            value = max;
        }
        return value;
    }

    private void showPopupWindow() {
        try {
            mPopupWindow.showAtLocation(mRootView, Gravity.TOP | Gravity.START, mX, mY);
        } catch (WindowManager.BadTokenException e) {
            // Intentionally ignore BadTokenException. This can happen in a real edge case where
            // parent.getWindowToken is not valid. See http://crbug.com/826052.
        }
    }
}
