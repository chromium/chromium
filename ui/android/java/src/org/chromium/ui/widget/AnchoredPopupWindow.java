// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.View.OnTouchListener;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;
import android.widget.PopupWindow;
import android.widget.PopupWindow.OnDismissListener;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.StyleRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.ui.R;
import org.chromium.ui.base.LocalizationUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * UI component that handles showing a {@link PopupWindow}. Positioning this popup happens through a
 * {@link RectProvider} provided during construction.
 */
public class AnchoredPopupWindow implements OnTouchListener, RectProvider.Observer {
    private static final int MINIMAL_POPUP_HEIGHT_DIP = 50; // 48dp touch target plus 1dp margin.
    private static final int MINIMAL_POPUP_WIDTH_DIP = 50; // 48dp touch target plus 1dp margin.

    /** An observer that is notified of AnchoredPopupWindow layout changes. */
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
    @IntDef({
        VerticalOrientation.MAX_AVAILABLE_SPACE,
        VerticalOrientation.BELOW,
        VerticalOrientation.ABOVE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface VerticalOrientation {
        /**
         * Vertically position to whichever side of the anchor has more available space. The popup
         * will be sized to ensure it fits on screen.
         */
        int MAX_AVAILABLE_SPACE = 0;

        /** Position below the anchor if there is enough space. */
        int BELOW = 1;

        /** Position above the anchor if there is enough space. */
        int ABOVE = 2;
    }

    /** HorizontalOrientation preferences for the popup */
    @IntDef({
        HorizontalOrientation.MAX_AVAILABLE_SPACE,
        HorizontalOrientation.CENTER,
        HorizontalOrientation.LAYOUT_DIRECTION
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface HorizontalOrientation {
        /**
         * Horizontally position to whichever side of the anchor has more available space. The popup
         * will be sized to ensure it fits on screen.
         */
        int MAX_AVAILABLE_SPACE = 0;

        /**
         * Horizontally center with respect to the anchor, so long as the popup still fits on the
         * screen.
         */
        int CENTER = 1;

        /**
         * Horizontally position to side as defined by @{@link LocalizationUtils#isLayoutRtl()}. The
         * popup will be sized to ensure it fits on screen.
         */
        int LAYOUT_DIRECTION = 2;
    }

    /**
     * Helper class holds information of popup window (e.g. rect on screen, position to anchorRect)
     */
    @VisibleForTesting
    static class PopupSpec {
        /** Whether the popup window shows below the anchored rect. */
        public final boolean isPositionBelow;

        /** Whether the popup window shows to the left of the anchored rect. */
        public final boolean isPositionToLeft;

        /** Location of the popup window in the current application window. */
        public final Rect popupRect;

        private PopupSpec(Rect rect, boolean isPositionBelow, boolean isPositionToLeft) {
            this.popupRect = rect;
            this.isPositionBelow = isPositionBelow;
            this.isPositionToLeft = isPositionToLeft;
        }
    }

    private static class RootViewRectProvider extends RectProvider
            implements View.OnLayoutChangeListener {
        private final View mRootView;

        RootViewRectProvider(View rootView) {
            mRootView = rootView;
            mRootView.addOnLayoutChangeListener(this);
            cacheWindowVisibleDisplayFrameRect();
        }

        @Override
        public void onLayoutChange(
                View v,
                int left,
                int top,
                int right,
                int bottom,
                int oldLeft,
                int oldTop,
                int oldRight,
                int oldBottom) {
            cacheWindowVisibleDisplayFrameRect();
            // TODO(crbug.com/40253505): call notifyRectChanged() if consumers don't do it.
        }

        private void cacheWindowVisibleDisplayFrameRect() {
            mRootView.getWindowVisibleDisplayFrame(mRect);

            // In multi-window, the coordinates of root view will be different than (0,0).
            // So we translate the coordinates of |mRect| w.r.t. its window. This ensures the
            // |mRect| always starts at (0,0).
            int[] rootCoordinates = new int[2];
            mRootView.getLocationOnScreen(rootCoordinates);
            mRect.offset(-rootCoordinates[0], -rootCoordinates[1]);
        }
    }

    // Cache Rect objects for querying View and Screen coordinate APIs.
    private final Rect mCachedPaddingRect = new Rect();

    // Spec of last shown popup window, or place holder value if the popup hasn't been shown yet.
    private PopupSpec mPopupSpec;

    private final Context mContext;
    private final Handler mHandler;
    private final View mRootView;
    private final RectProvider mViewportRectProvider;

    /** The actual {@link PopupWindow}.  Internalized to prevent API leakage. */
    private final PopupWindow mPopupWindow;

    /** Provides the {@link Rect} to anchor the popup to in screen space. */
    private final RectProvider mRectProvider;

    private final Runnable mDismissRunnable =
            new Runnable() {
                @Override
                public void run() {
                    if (mPopupWindow.isShowing()) dismiss();
                }
            };

    private final OnDismissListener mDismissListener =
            new OnDismissListener() {
                @Override
                public void onDismiss() {
                    if (mIgnoreDismissal) return;

                    mHandler.removeCallbacks(mDismissRunnable);
                    for (OnDismissListener listener : mDismissListeners) listener.onDismiss();

                    mRectProvider.stopObserving();
                    mViewportRectProvider.stopObserving();
                }
            };

    private boolean mDismissOnTouchInteraction;

    // Pass through for the internal PopupWindow.  This class needs to intercept these for API
    // purposes, but they are still useful to callers.
    private ObserverList<OnDismissListener> mDismissListeners = new ObserverList<>();
    private OnTouchListener mTouchListener;
    private LayoutObserver mLayoutObserver;

    /** The margin to add to the popup so it doesn't bump against the edges of the screen. */
    private int mMarginPx;

    /**
     * The maximum width of the popup. This width is used as long as the popup still fits on screen.
     */
    private int mMaxWidthPx;

    /** The desired width for the content. */
    private int mDesiredContentWidth;

    // Preferred orientation for the popup with respect to the anchor.
    // Preferred vertical orientation for the popup with respect to the anchor.
    @VerticalOrientation
    private int mPreferredVerticalOrientation = VerticalOrientation.MAX_AVAILABLE_SPACE;

    // Preferred horizontal orientation for the popup with respect to the anchor.
    @HorizontalOrientation
    private int mPreferredHorizontalOrientation = HorizontalOrientation.MAX_AVAILABLE_SPACE;

    /**
     * Tracks whether or not we are in the process of updating the popup, which might include a
     * dismiss and show.  In that case we don't want to let the world know we're dismissing because
     * it's only temporary.
     */
    private boolean mIgnoreDismissal;

    private boolean mVerticalOverlapAnchor;
    private boolean mHorizontalOverlapAnchor;
    private boolean mUpdateOrientationOnChange;
    private boolean mSmartAnchorWithMaxWidth;

    private @StyleRes int mAnimationStyleId;
    private boolean mAnimateFromAnchor;

    public AnchoredPopupWindow(
            Context context,
            View rootView,
            Drawable background,
            View contentView,
            RectProvider anchorRectProvider) {
        this(context, rootView, background, contentView, anchorRectProvider, null);
    }

    /**
     * Constructs an {@link AnchoredPopupWindow} instance.
     *
     * @param context Context to draw resources from.
     * @param rootView The {@link View} to use for size calculations and for display.
     * @param background The background {@link Drawable} to use for the popup.
     * @param contentView The content view to set on the popup. Expected to be a {@link ViewGroup}.
     * @param anchorRectProvider The {@link RectProvider} that will provide the {@link Rect} this
     *     popup attaches and orients to. The coordinates in the {@link Rect} are expected to be
     *     screen coordinates.
     * @param viewportRectProvider The {@link RectProvider} that provides the {@link Rect} for the
     *     visible viewpoint. If null, the window coordinates of the root view will be used.
     */
    public AnchoredPopupWindow(
            Context context,
            View rootView,
            Drawable background,
            View contentView,
            RectProvider anchorRectProvider,
            @Nullable RectProvider viewportRectProvider) {
        mContext = context;
        mRootView = rootView.getRootView();
        mViewportRectProvider =
                viewportRectProvider != null
                        ? viewportRectProvider
                        : new RootViewRectProvider(mRootView);
        mPopupWindow = UiWidgetFactory.getInstance().createPopupWindow(mContext);
        mHandler = new Handler();
        mRectProvider = anchorRectProvider;
        mPopupSpec = new PopupSpec(new Rect(), false, false);

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
        mViewportRectProvider.startObserving(this);

        updatePopupLayout();
        if (hasMinimalSize()) showPopupWindow();
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
     *                happen for both taps inside and outside the popup except when a tap is handled
     *                by child views. The default is {@code false}.
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
     * Sets the animation style for the popup. This should be called before the popup is shown.
     * Setting this style will take precedence over {@link #setAnimateFromAnchor(boolean)}.
     * @param animationStyleId The id of the animation style.
     */
    public void setAnimationStyle(int animationStyleId) {
        mAnimationStyleId = animationStyleId;
        mPopupWindow.setAnimationStyle(animationStyleId);
    }

    /**
     * Set whether the popup should enter from / exit to the anchor point. This should be
     * called before the popup is shown. If an animation style is specified by
     * {@link #setAnimationStyle(int)}, this method will have no effect.
     * @param animateFromAnchor Whether the popup should animator from anchor point.
     */
    public void setAnimateFromAnchor(boolean animateFromAnchor) {
        mAnimateFromAnchor = animateFromAnchor;
    }

    /**
     * If set to true, orientation will be updated every time that the {@link OnRectChanged} is
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
        final float density = mRootView.getResources().getDisplayMetrics().density;
        mMaxWidthPx = Math.max(maxWidth, (int) Math.ceil(density * MINIMAL_POPUP_WIDTH_DIP));
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
     * Set whether we want position the context menu around the anchor to maximize its width.
     *
     * If this is set, in some cases the popup will be shown differently than the settings for
     * {@link #setVerticalOverlapAnchor} and {@link #setHorizontalOverlapAnchor}.
     * (e.g. Side of anchor is too narrow to make it not horizontally smartAnchor).
     *
     * This should be called before the popup is shown.
     * @param smartAnchor Whether the popup should smartAnchor the anchor.
     */
    public void setSmartAnchorWithMaxWidth(boolean smartAnchor) {
        mSmartAnchorWithMaxWidth = smartAnchor;
    }

    /**
     * Changes the background of the popup.
     * @param background The {@link Drawable} that is set to be background.
     */
    public void setBackgroundDrawable(Drawable background) {
        mPopupWindow.setBackgroundDrawable(background);
    }

    /** Sets the elevation of the popup. */
    public void setElevation(float elevation) {
        mPopupWindow.setElevation(elevation);
    }

    /**
     * Sets the width for the content of the popup window. The popup window will be shown in this
     * exact width unless certain constraint presents (e.g. desiredContentWidth > maxWidthPx).
     */
    public void setDesiredContentWidth(int width) {
        mDesiredContentWidth = width;
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
        // If the root view is not attached to the Window, this may result in an
        // IllegalArgumentException. Regardless, sizing the popup won't work properly so exit early.
        // See https://crbug.com/1212602 for details.
        if (!mRootView.isAttachedToWindow()) return;

        mPopupWindow.getBackground().getPadding(mCachedPaddingRect);
        int paddingX = mCachedPaddingRect.left + mCachedPaddingRect.right;
        int paddingY = mCachedPaddingRect.top + mCachedPaddingRect.bottom;

        Rect anchorRect = mRectProvider.getRect();
        boolean currentPositionBelow = mPopupSpec.isPositionBelow;
        boolean currentPositionToLeft = mPopupSpec.isPositionToLeft;
        boolean preferCurrentOrientation = mPopupWindow.isShowing() && !mUpdateOrientationOnChange;

        // Determine whether or not the popup should be above or below the anchor.
        // Aggressively try to put it below the anchor.  Put it above only if it would fit better.
        View contentView = mPopupWindow.getContentView();

        mPopupSpec =
                calculatePopupWindowSpec(
                        mViewportRectProvider.getRect(),
                        anchorRect,
                        contentView,
                        mRootView.getWidth(),
                        paddingX,
                        paddingY,
                        mMarginPx,
                        mMaxWidthPx,
                        mDesiredContentWidth,
                        mPreferredHorizontalOrientation,
                        mPreferredVerticalOrientation,
                        currentPositionBelow,
                        currentPositionToLeft,
                        preferCurrentOrientation,
                        mHorizontalOverlapAnchor,
                        mVerticalOverlapAnchor,
                        mSmartAnchorWithMaxWidth);

        boolean isPositionBelow = mPopupSpec.isPositionBelow;
        Rect popupRect = mPopupSpec.popupRect;
        if (mLayoutObserver != null) {
            mLayoutObserver.onPreLayoutChange(
                    isPositionBelow,
                    popupRect.left,
                    popupRect.top,
                    popupRect.width(),
                    popupRect.height(),
                    anchorRect);
        }

        if (mPopupWindow.isShowing() && !hasMinimalSize()) {
            mPopupWindow.dismiss();
        } else if (mPopupWindow.isShowing() && isPositionBelow != currentPositionBelow) {
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

        if (hasMinimalSize()) {
            mPopupWindow.update(
                    popupRect.left, popupRect.top, popupRect.width(), popupRect.height());
        }
    }

    /**
     * Helps to figure out whether the actual pixel size is sufficient that users see what they are
     * tapping. Popups can be very narrow (e.g. in landscape) and still be interactive.
     * @return True iff the popup is large enough to be safely shown to users.
     */
    private boolean hasMinimalSize() {
        final float density = mRootView.getResources().getDisplayMetrics().density;
        return mPopupSpec.popupRect.height() >= density * MINIMAL_POPUP_HEIGHT_DIP
                && mPopupSpec.popupRect.width() >= density * MINIMAL_POPUP_WIDTH_DIP;
    }

    /**
     * Calculate the Rect where the popup window will displayed on the current application window.
     *
     * @param freeSpaceRect The rect representing the window size. Always starts from (0,0) as top
     *     left.
     * @param anchorRect The rect that popup anchored to in the window.
     * @param contentView The content view of popup window. Expected to be a {@link ViewGroup}.
     * @param rootViewWidth The width of root view.
     * @param paddingX The padding on the X axis of popup window.
     * @param paddingY The padding on the Y axis of popup window.
     * @param marginPx Value set by {@link #setMargin(int)}.
     * @param maxWidthPx Value set by {@link #setMaxWidth(int)}.
     * @param desiredContentWidth Value set by {@link #setDesiredContentWidth(int)}.
     * @param preferredHorizontalOrientation Value set by {@link
     *     #setPreferredHorizontalOrientation(int)}.
     * @param preferredVerticalOrientation Value set by {@link
     *     #setPreferredVerticalOrientation(int)}.
     * @param currentPositionBelow Whether the currently shown popup window is presented below
     *     anchored rect.
     * @param currentPositionToLeft Whether the currently shown popup window is presented to the
     *     left of anchored rect.
     * @param preferCurrentOrientation Whether prefer to reserve the popup orientation. If this set
     *     to true, popup window will prefer to show below / to left of anchored window the same way
     *     as |currentPositionBelow| and |currentPositionToLeft|.
     * @param horizontalOverlapAnchor Value set by {@link #setHorizontalOverlapAnchor(boolean)}.
     * @param verticalOverlapAnchor Value set by {@link #setVerticalOverlapAnchor(boolean)}.
     * @param smartAnchorWithMaxWidth Value set by {@link #setSmartAnchorWithMaxWidth(boolean)}.
     * @return {@link PopupSpec} that includes the popup specs (e.g. location in window)
     */
    @VisibleForTesting
    static PopupSpec calculatePopupWindowSpec(
            final Rect freeSpaceRect,
            final Rect anchorRect,
            final View contentView,
            final int rootViewWidth,
            int paddingX,
            int paddingY,
            int marginPx,
            int maxWidthPx,
            int desiredContentWidth,
            @HorizontalOrientation int preferredHorizontalOrientation,
            @VerticalOrientation int preferredVerticalOrientation,
            boolean currentPositionBelow,
            boolean currentPositionToLeft,
            boolean preferCurrentOrientation,
            boolean horizontalOverlapAnchor,
            boolean verticalOverlapAnchor,
            boolean smartAnchorWithMaxWidth) {
        // Determine the size of the text popup.
        final int maxContentWidth =
                getMaxContentWidth(maxWidthPx, rootViewWidth, marginPx, paddingX);
        final int widthSpec =
                desiredContentWidth > 0
                        ? MeasureSpec.makeMeasureSpec(
                                Math.min(desiredContentWidth, maxContentWidth), MeasureSpec.EXACTLY)
                        : MeasureSpec.makeMeasureSpec(maxContentWidth, MeasureSpec.AT_MOST);

        contentView.measure(widthSpec, MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
        final int idealContentHeight = contentView.getMeasuredHeight();
        final int idealContentWidth = contentView.getMeasuredWidth();

        // Width adjustments based on the anchor and settings.
        boolean isPositionToLeft = currentPositionToLeft;
        boolean allowHorizontalOverlap = horizontalOverlapAnchor;
        boolean allowVerticalOverlap = verticalOverlapAnchor;
        if (preferredHorizontalOrientation == HorizontalOrientation.MAX_AVAILABLE_SPACE) {
            int spaceLeftOfAnchor =
                    getSpaceLeftOfAnchor(anchorRect, freeSpaceRect, allowHorizontalOverlap);
            int spaceRightOfAnchor =
                    getSpaceRightOfAnchor(anchorRect, freeSpaceRect, allowHorizontalOverlap);
            isPositionToLeft =
                    shouldPositionLeftOfAnchor(
                            spaceLeftOfAnchor,
                            spaceRightOfAnchor,
                            idealContentWidth + paddingX + marginPx,
                            currentPositionToLeft,
                            preferCurrentOrientation);

            int idealWidthAroundAnchor = isPositionToLeft ? spaceLeftOfAnchor : spaceRightOfAnchor;
            if (idealWidthAroundAnchor < maxContentWidth && smartAnchorWithMaxWidth) {
                allowHorizontalOverlap = true;
                allowVerticalOverlap = false;
            }
        } else if (preferredHorizontalOrientation == HorizontalOrientation.LAYOUT_DIRECTION) {
            isPositionToLeft = LocalizationUtils.isLayoutRtl();
        }

        // Height adjustment based on anchorRect and settings.

        // TODO(dtrainor): This follows the previous logic.  But we should look into if we want to
        // use the root view dimensions instead of the window dimensions here so the popup can't
        // bleed onto the decorations.
        final int spaceAboveAnchor =
                (allowVerticalOverlap ? anchorRect.bottom : anchorRect.top)
                        - freeSpaceRect.top
                        - paddingY
                        - marginPx;
        final int spaceBelowAnchor =
                freeSpaceRect.bottom
                        - (allowVerticalOverlap ? anchorRect.top : anchorRect.bottom)
                        - paddingY
                        - marginPx;

        // Bias based on the center of the popup and where it is on the screen.
        final boolean idealFitsBelow = idealContentHeight <= spaceBelowAnchor;
        final boolean idealFitsAbove = idealContentHeight <= spaceAboveAnchor;

        // Position the popup in the largest available space where it can fit.  This will bias the
        // popups to show below the anchor if it will not fit in either place.
        // TODO(crbug.com/40831291): Address cases where spaceBelowAnchor = 0, popup is still
        // biased to anchored below the rect.
        boolean isPositionBelow =
                (idealFitsBelow && spaceBelowAnchor >= spaceAboveAnchor) || !idealFitsAbove;

        // Override the ideal popup orientation if we are trying to maintain the current one.
        if (preferCurrentOrientation && currentPositionBelow != isPositionBelow) {
            if (currentPositionBelow && idealFitsBelow) isPositionBelow = true;
            if (!currentPositionBelow && idealFitsAbove) isPositionBelow = false;
        }

        if (preferredVerticalOrientation == VerticalOrientation.BELOW && idealFitsBelow) {
            isPositionBelow = true;
        }
        if (preferredVerticalOrientation == VerticalOrientation.ABOVE && idealFitsAbove) {
            isPositionBelow = false;
        }

        final int maxContentHeight = isPositionBelow ? spaceBelowAnchor : spaceAboveAnchor;
        final int heightMeasureSpec =
                MeasureSpec.makeMeasureSpec(maxContentHeight, MeasureSpec.AT_MOST);
        contentView.measure(widthSpec, heightMeasureSpec);

        int width = contentView.getMeasuredWidth() + paddingX;
        int height = contentView.getMeasuredHeight() + paddingY;

        // Calculate the width of the contentView by adding the width of its children, their margin,
        // and its own padding. This is necessary when a TextView overflows to multiple lines
        // because the contentView(parent) would return the maximum available width, which is larger
        // than the actual needed width.
        ViewGroup parent = (ViewGroup) contentView;
        boolean isHorizontalLinearLayout =
                parent instanceof LinearLayout
                        && ((LinearLayout) parent).getOrientation() == LinearLayout.HORIZONTAL;
        if (isHorizontalLinearLayout && parent.getChildCount() > 0) {
            int contentMeasuredWidth = contentView.getPaddingStart() + contentView.getPaddingEnd();
            for (int index = 0; index < parent.getChildCount(); index++) {
                View childView = parent.getChildAt(index);
                int childWidth = childView.getMeasuredWidth();
                if (childWidth > 0) {
                    contentMeasuredWidth += childWidth;
                    LayoutParams lp = (LayoutParams) childView.getLayoutParams();
                    contentMeasuredWidth += lp.leftMargin + lp.rightMargin;
                }
            }
            width = contentMeasuredWidth + paddingX;
        }

        // Determine the position of the text popup.
        final int popupX =
                getPopupX(
                        anchorRect,
                        freeSpaceRect,
                        width,
                        marginPx,
                        allowHorizontalOverlap,
                        preferredHorizontalOrientation,
                        isPositionToLeft);
        final int popupY = getPopupY(anchorRect, height, allowVerticalOverlap, isPositionBelow);

        return new PopupSpec(
                new Rect(popupX, popupY, popupX + width, popupY + height),
                isPositionBelow,
                isPositionToLeft);
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
    static boolean shouldPositionLeftOfAnchor(
            int spaceToLeftOfAnchor,
            int spaceToRightOfAnchor,
            int idealPopupWidth,
            boolean currentPositionToLeft,
            boolean preferCurrentOrientation) {
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
    static int getPopupX(
            Rect anchorRect,
            Rect windowRect,
            int popupWidth,
            int marginPx,
            boolean overlapAnchor,
            @HorizontalOrientation int horizontalOrientation,
            boolean positionToLeft) {
        int x;

        if (horizontalOrientation == HorizontalOrientation.CENTER) {
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

    // TODO(crbug.com/40831293): Account margin when position above the anchor.
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
        boolean touchInterceptedByClient =
                mTouchListener != null && mTouchListener.onTouch(v, event);

        RecordUserAction.record(
                event.getAction() == MotionEvent.ACTION_OUTSIDE
                        ? "InProductHelp.OutsideTouch"
                        : "InProductHelp.InsideTouch");

        if (mDismissOnTouchInteraction) {
            // Pass down the touch event to child views. If the content view has clickable children,
            // make sure we give them the opportunity to trigger.
            // TODO(crbug.com/40806699): Revisit handling touches on content when
            // mDismissOnTouchInteraction is true.
            boolean touchInterceptedByChild =
                    !touchInterceptedByClient
                            && mPopupWindow.getContentView().dispatchTouchEvent(event);
            if (!touchInterceptedByChild) dismiss();
        }

        return touchInterceptedByClient;
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

    /**
     * Calculate the style Id to use when showing the popup window,
     * when {@link #setAnimateFromAnchor(true)}.
     * @param isPositionBelow Whether the popup is positioned below the anchor rect.
     * @param isPositionToLeft Whether the popup is positioned below the anchor rect.
     * @return The style resource Id to use for {@link PopupWindow#setAnimationStyle}
     */
    @VisibleForTesting
    static @StyleRes int calculateAnimationStyle(
            boolean isPositionBelow, boolean isPositionToLeft) {
        if (isPositionToLeft) {
            return isPositionBelow
                    ? R.style.AnchoredPopupAnimEndTop // Left + below -> enter top right (end)
                    : R.style.AnchoredPopupAnimEndBottom; // Left + above -> enter bottom right
            // (end)
        }
        return isPositionBelow
                ? R.style.AnchoredPopupAnimStartTop // Right & below -> enter top left (start)
                : R.style.AnchoredPopupAnimStartBottom; // Right & above -> enter bottom left
        // (start)
    }

    @VisibleForTesting
    void showPopupWindow() {
        if (mAnimateFromAnchor && mAnimationStyleId == 0) {
            int animationStyle =
                    calculateAnimationStyle(
                            mPopupSpec.isPositionBelow, mPopupSpec.isPositionToLeft);
            mPopupWindow.setAnimationStyle(animationStyle);
        }
        try {
            assert hasMinimalSize();
            mPopupWindow.showAtLocation(
                    mRootView,
                    Gravity.TOP | Gravity.START,
                    mPopupSpec.popupRect.left,
                    mPopupSpec.popupRect.top);
        } catch (WindowManager.BadTokenException e) {
            // Intentionally ignore BadTokenException. This can happen in a real edge case where
            // parent.getWindowToken is not valid. See http://crbug.com/826052.
        }
    }
}
