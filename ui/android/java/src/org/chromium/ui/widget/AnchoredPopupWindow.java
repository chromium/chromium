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
import android.view.View.OnTouchListener;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.PopupWindow;
import android.widget.PopupWindow.OnDismissListener;

import androidx.annotation.IntDef;
import androidx.annotation.StyleRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.R;
import org.chromium.ui.base.LocalizationUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.Supplier;

/**
 * UI component that handles showing a {@link PopupWindow}. Positioning this popup happens through a
 * {@link RectProvider} provided during construction.
 */
@NullMarked
public class AnchoredPopupWindow implements OnTouchListener, RectProvider.Observer {
    private static final int MIN_TOUCHABLE_HEIGHT_DIP = 50; // 48dp touch target plus 1dp margin.
    private static final int MIN_TOUCHABLE_WIDTH_DIP = 50; // 48dp touch target plus 1dp margin.

    /** An observer that is notified of AnchoredPopupWindow layout changes. */
    public interface LayoutObserver {
        /**
         * Called immediately before the popup layout changes.
         *
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
    private final SpecCalculator mSpecCalculator;

    /** The actual {@link PopupWindow}. Internalized to prevent API leakage. */
    private final PopupWindow mPopupWindow;

    /** Provides the {@link Rect} to anchor the popup to in screen space. */
    private final RectProvider mRectProvider;

    private final Supplier<View> mContentViewCreator;

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
                    if (mBeingDismissedByTouch) {
                        // Leave mDismissedByInsideTouch untouched.
                        mBeingDismissedByTouch = false;
                    } else {
                        // It is dismissed by another way. Clear mDismissedByInsideTouch.
                        mDismissedByInsideTouch = false;
                    }

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
    private final ObserverList<OnDismissListener> mDismissListeners = new ObserverList<>();
    private @Nullable OnTouchListener mTouchListener;
    private @Nullable LayoutObserver mLayoutObserver;
    private @Nullable View mContentView;

    /** The margin to add to the popup so it doesn't bump against the edges of the screen. */
    private int mMarginPx;

    /**
     * The maximum width of the popup. This width is used as long as the popup still fits on screen.
     */
    private int mMaxWidthPx;

    /** The desired width for the content. */
    private int mDesiredContentWidth;

    /** The desired height for the content. */
    private int mDesiredContentHeight;

    // Preferred orientation for the popup with respect to the anchor.
    // Preferred vertical orientation for the popup with respect to the anchor.
    @VerticalOrientation
    private int mPreferredVerticalOrientation = VerticalOrientation.MAX_AVAILABLE_SPACE;

    // Preferred horizontal orientation for the popup with respect to the anchor.
    @HorizontalOrientation
    private int mPreferredHorizontalOrientation = HorizontalOrientation.MAX_AVAILABLE_SPACE;

    /**
     * Tracks whether or not we are in the process of updating the popup, which might include a
     * dismiss and show. In that case we don't want to let the world know we're dismissing because
     * it's only temporary.
     */
    private boolean mIgnoreDismissal;

    private boolean mVerticalOverlapAnchor;
    private boolean mHorizontalOverlapAnchor;
    private boolean mUpdateOrientationOnChange;
    private boolean mSmartAnchorWithMaxWidth;
    private boolean mAllowNonTouchableSize;

    private boolean mBeingDismissedByTouch;
    private boolean mDismissedByInsideTouch;

    private @StyleRes int mAnimationStyleId;
    private boolean mAnimateFromAnchor;
    private boolean mDismissOnScreenSizeChange;
    private @Nullable WindowBoundsChangeDetector mWindowBoundsChangeDetector;

    /** A builder for {@link AnchoredPopupWindow} instances. */
    public static class Builder {
        private final Context mContext;
        private final View mRootView;
        private final Supplier<View> mContentViewCreator;
        private final RectProvider mAnchorRectProvider;
        private final Drawable mBackground;

        private @Nullable RectProvider mViewportRectProvider;
        private @Nullable OnDismissListener mOnDismissListener;
        private @Nullable OnTouchListener mTouchListener;
        private @Nullable LayoutObserver mLayoutObserver;
        private @Nullable SpecCalculator mSpecCalculator;

        private int mMarginPx;
        private int mMaxWidthPx;
        private int mDesiredContentWidthPx;
        private int mDesiredContentHeightPx;

        @VerticalOrientation
        private int mPreferredVerticalOrientation = VerticalOrientation.MAX_AVAILABLE_SPACE;

        @HorizontalOrientation
        private int mPreferredHorizontalOrientation = HorizontalOrientation.MAX_AVAILABLE_SPACE;

        private boolean mDismissOnTouchInteraction;
        private boolean mDismissOnScreenSizeChange;
        private boolean mVerticalOverlapAnchor;
        private boolean mHorizontalOverlapAnchor;
        private boolean mUpdateOrientationOnChange;
        private boolean mSmartAnchorWithMaxWidth;
        private boolean mAllowNonTouchableSize;
        private @StyleRes int mAnimationStyleId;
        private boolean mAnimateFromAnchor;
        private boolean mFocusable;
        private float mElevation;
        private boolean mTouchModal;
        private boolean mOutsideTouchable;
        private boolean mIsOutsideTouchableSet;
        private int mWindowLayoutType;
        private boolean mIsWindowLayoutTypeSet;

        /**
         * Constructs an {@link AnchoredPopupWindow} instance.
         *
         * @param context Context to draw resources from.
         * @param rootView The {@link View} to use for size calculations and for display.
         * @param background The background {@link Drawable} to use for the popup.
         * @param contentViewCreator The supplier for the content view to set on the popup. The view
         *     is expected to be a {@link ViewGroup}.
         * @param anchorRectProvider The {@link RectProvider} that will provide the {@link Rect}
         *     this popup attaches and orients to. The coordinates in the {@link Rect} are expected
         *     to be screen coordinates.
         * @deprecated Use the {@link Builder} to create the popup instead.
         */
        public Builder(
                Context context,
                View rootView,
                Drawable background,
                Supplier<View> contentViewCreator,
                RectProvider anchorRectProvider) {
            mContext = context;
            mRootView = rootView;
            mBackground = background;
            mContentViewCreator = contentViewCreator;
            mAnchorRectProvider = anchorRectProvider;
        }

        /**
         * @param viewportRectProvider The {@link RectProvider} that provides the {@link Rect} for
         *     the visible viewpoint. If null, the window coordinates of the root view will be used.
         */
        public Builder setViewportRectProvider(RectProvider viewportRectProvider) {
            mViewportRectProvider = viewportRectProvider;
            return this;
        }

        /**
         * @param onDismissListener A listener to be called when the popup is dismissed.
         */
        public Builder addOnDismissListener(OnDismissListener onDismissListener) {
            mOnDismissListener = onDismissListener;
            return this;
        }

        /**
         * @param onTouchListener A callback for all touch events being dispatched to the popup.
         */
        public Builder setTouchInterceptor(OnTouchListener onTouchListener) {
            mTouchListener = onTouchListener;
            return this;
        }

        /**
         * @param layoutObserver The observer to be notified of layout changes.
         */
        public Builder setLayoutObserver(LayoutObserver layoutObserver) {
            mLayoutObserver = layoutObserver;
            return this;
        }

        /**
         * @param calculator The calculator that can customize the positioning behavior.
         */
        public Builder setSpecCalculator(SpecCalculator calculator) {
            mSpecCalculator = calculator;
            return this;
        }

        /**
         * @param margin The vertical and horizontal margin in pixels.
         */
        public Builder setMargin(int margin) {
            mMarginPx = margin;
            return this;
        }

        /**
         * @param maxWidth The max width for the popup.
         */
        public Builder setMaxWidth(int maxWidth) {
            mMaxWidthPx = maxWidth;
            return this;
        }

        /**
         * @param width The desired width for the content of the popup window in pixel.
         * @param height The desired height for the content of the popup window in pixel.
         */
        public Builder setDesiredContentSize(int width, int height) {
            mDesiredContentWidthPx = width;
            mDesiredContentHeightPx = height;
            return this;
        }

        /**
         * @param width The desired width for the content of the popup window in pixel.
         */
        public Builder setDesiredContentWidth(int width) {
            mDesiredContentWidthPx = width;
            return this;
        }

        /**
         * @param height The desired height for the content of the popup window in pixel.
         */
        public Builder setDesiredContentHeight(int height) {
            mDesiredContentHeightPx = height;
            return this;
        }

        /**
         * @param orientation The vertical orientation preferred.
         */
        public Builder setPreferredVerticalOrientation(@VerticalOrientation int orientation) {
            mPreferredVerticalOrientation = orientation;
            return this;
        }

        /**
         * @param orientation The horizontal orientation preferred.
         */
        public Builder setPreferredHorizontalOrientation(@HorizontalOrientation int orientation) {
            mPreferredHorizontalOrientation = orientation;
            return this;
        }

        /**
         * @param dismiss Whether or not to dismiss this popup when the screen is tapped.
         */
        public Builder setDismissOnTouchInteraction(boolean dismiss) {
            mDismissOnTouchInteraction = dismiss;
            return this;
        }

        /**
         * @param dismiss Whether or not to dismiss this popup when the screen size changes.
         */
        public Builder setDismissOnScreenSizeChange(boolean dismiss) {
            mDismissOnScreenSizeChange = dismiss;
            return this;
        }

        /**
         * @param overlap Whether the popup should vertically overlap the anchor.
         */
        public Builder setVerticalOverlapAnchor(boolean overlap) {
            mVerticalOverlapAnchor = overlap;
            return this;
        }

        /**
         * @param overlap Whether the popup should horizontally overlap the anchor.
         */
        public Builder setHorizontalOverlapAnchor(boolean overlap) {
            mHorizontalOverlapAnchor = overlap;
            return this;
        }

        /**
         * @param update If set to true, orientation will be updated every time that the {@link
         *     OnRectChanged} is called.
         */
        public Builder setUpdateOrientationOnChange(boolean update) {
            mUpdateOrientationOnChange = update;
            return this;
        }

        /**
         * @param smartAnchor Whether the popup should smartAnchor the anchor.
         */
        public Builder setSmartAnchorWithMaxWidth(boolean smartAnchor) {
            mSmartAnchorWithMaxWidth = smartAnchor;
            return this;
        }

        /**
         * @param allow Whether to allow the popup to have a small non-touchable size.
         */
        public Builder setAllowNonTouchableSize(boolean allow) {
            mAllowNonTouchableSize = allow;
            return this;
        }

        /**
         * @param animationStyleId The id of the animation style.
         */
        public Builder setAnimationStyle(@StyleRes int animationStyleId) {
            mAnimationStyleId = animationStyleId;
            return this;
        }

        /**
         * @param animateFromAnchor Whether the popup should animator from anchor point.
         */
        public Builder setAnimateFromAnchor(boolean animateFromAnchor) {
            mAnimateFromAnchor = animateFromAnchor;
            return this;
        }

        /**
         * @param focusable True if the popup is focusable, false otherwise.
         */
        public Builder setFocusable(boolean focusable) {
            mFocusable = focusable;
            return this;
        }

        /**
         * @param elevation The elevation of the popup.
         */
        public Builder setElevation(float elevation) {
            mElevation = elevation;
            return this;
        }

        /**
         * @param touchModal True if the popup is touch modal, false otherwise.
         */
        public Builder setTouchModal(boolean touchModal) {
            mTouchModal = touchModal;
            return this;
        }

        /**
         * @param touchable True if the popup is outside touchable, false otherwise.
         */
        public Builder setOutsideTouchable(boolean touchable) {
            mOutsideTouchable = touchable;
            mIsOutsideTouchableSet = true;
            return this;
        }

        /**
         * @param layoutType The window layout type of the window.
         */
        public Builder setWindowLayoutType(int layoutType) {
            mWindowLayoutType = layoutType;
            mIsWindowLayoutTypeSet = true;
            return this;
        }

        /**
         * @return A new {@link AnchoredPopupWindow}.
         */
        public AnchoredPopupWindow build() {
            return new AnchoredPopupWindow(this);
        }
    }

    private AnchoredPopupWindow(Builder builder) {
        this(
                builder.mContext,
                builder.mRootView,
                builder.mBackground,
                builder.mContentViewCreator,
                builder.mAnchorRectProvider,
                builder.mViewportRectProvider,
                builder.mSpecCalculator);

        if (builder.mOnDismissListener != null) {
            addOnDismissListener(builder.mOnDismissListener);
        }
        setTouchInterceptor(builder.mTouchListener);
        setLayoutObserver(builder.mLayoutObserver);
        setMargin(builder.mMarginPx);
        if (builder.mMaxWidthPx > 0) setMaxWidth(builder.mMaxWidthPx);
        if (builder.mDesiredContentWidthPx != 0 || builder.mDesiredContentHeightPx != 0) {
            updateDesiredContentSize(
                    builder.mDesiredContentWidthPx, builder.mDesiredContentHeightPx, false);
        }
        setPreferredVerticalOrientation(builder.mPreferredVerticalOrientation);
        setPreferredHorizontalOrientation(builder.mPreferredHorizontalOrientation);
        setDismissOnTouchInteraction(builder.mDismissOnTouchInteraction);
        setDismissOnScreenSizeChange(builder.mDismissOnScreenSizeChange);
        setVerticalOverlapAnchor(builder.mVerticalOverlapAnchor);
        setHorizontalOverlapAnchor(builder.mHorizontalOverlapAnchor);
        setUpdateOrientationOnChange(builder.mUpdateOrientationOnChange);
        setSmartAnchorWithMaxWidth(builder.mSmartAnchorWithMaxWidth);
        setAllowNonTouchableSize(builder.mAllowNonTouchableSize);
        if (builder.mAnimationStyleId != 0) {
            setAnimationStyle(builder.mAnimationStyleId);
        }
        setAnimateFromAnchor(builder.mAnimateFromAnchor);
        setFocusable(builder.mFocusable);
        setElevation(builder.mElevation);
        setTouchModal(builder.mTouchModal);
        if (builder.mIsOutsideTouchableSet) {
            setOutsideTouchable(builder.mOutsideTouchable);
        }
        if (builder.mIsWindowLayoutTypeSet) {
            setWindowLayoutType(builder.mWindowLayoutType);
        }
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
     * @deprecated Use the {@link Builder} to create the popup instead.
     */
    @Deprecated
    public AnchoredPopupWindow(
            Context context,
            View rootView,
            Drawable background,
            View contentView,
            RectProvider anchorRectProvider) {
        this(context, rootView, background, () -> contentView, anchorRectProvider, null, null);
    }

    /**
     * Constructs an {@link AnchoredPopupWindow} instance.
     *
     * @param context Context to draw resources from.
     * @param rootView The {@link View} to use for size calculations and for display.
     * @param background The background {@link Drawable} to use for the popup.
     * @param contentViewCreator The supplier for the content view to set on the popup. The view is
     *     expected to be a {@link ViewGroup}.
     * @param anchorRectProvider The {@link RectProvider} that will provide the {@link Rect} this
     *     popup attaches and orients to. The coordinates in the {@link Rect} are expected to be
     *     screen coordinates.
     * @param viewportRectProvider The {@link RectProvider} that provides the {@link Rect} for the
     *     visible viewpoint. If null, the window coordinates of the root view will be used.
     * @deprecated Use the {@link Builder} to create the popup instead.
     */
    @Deprecated
    public AnchoredPopupWindow(
            Context context,
            View rootView,
            @Nullable Drawable background,
            Supplier<View> contentViewCreator,
            RectProvider anchorRectProvider,
            @Nullable RectProvider viewportRectProvider) {
        this(
                context,
                rootView,
                background,
                contentViewCreator,
                anchorRectProvider,
                viewportRectProvider,
                null);
    }

    /**
     * Constructs an {@link AnchoredPopupWindow} instance.
     *
     * @param context Context to draw resources from.
     * @param rootView The {@link View} to use for size calculations and for display.
     * @param background The background {@link Drawable} to use for the popup.
     * @param contentViewCreator The supplier for the content view to set on the popup. The view is
     *     expected to be a {@link ViewGroup}.
     * @param anchorRectProvider The {@link RectProvider} that will provide the {@link Rect} this
     *     popup attaches and orients to. The coordinates in the {@link Rect} are expected to be
     *     screen coordinates.
     * @param viewportRectProvider The {@link RectProvider} that provides the {@link Rect} for the
     *     visible viewpoint. If null, the window coordinates of the root view will be used.
     * @param calculator The {@link SpecCalculator} that can customize the positioning behavior.
     * @deprecated Use the {@link Builder} to create the popup instead.
     */
    @Deprecated
    public AnchoredPopupWindow(
            Context context,
            View rootView,
            @Nullable Drawable background,
            Supplier<View> contentViewCreator,
            RectProvider anchorRectProvider,
            @Nullable RectProvider viewportRectProvider,
            @Nullable SpecCalculator calculator) {
        mContext = context;
        mRootView = rootView.getRootView();
        mContentViewCreator = contentViewCreator;
        mViewportRectProvider =
                viewportRectProvider != null
                        ? viewportRectProvider
                        : new RootViewRectProvider(mRootView);
        if (calculator == null) {
            calculator = new PopupSpecCalculator();
        }
        mSpecCalculator = calculator;
        mPopupWindow = UiWidgetFactory.getInstance().createPopupWindow(mContext);
        mHandler = new Handler();
        mRectProvider = anchorRectProvider;
        mPopupSpec =
                new PopupSpec(
                        new Rect(), new PopupPositionParams(0, 0, false, false, false, false));

        mPopupWindow.setWidth(ViewGroup.LayoutParams.WRAP_CONTENT);
        mPopupWindow.setHeight(ViewGroup.LayoutParams.WRAP_CONTENT);
        mPopupWindow.setBackgroundDrawable(background);

        mPopupWindow.setTouchInterceptor(this);
        mPopupWindow.setOnDismissListener(mDismissListener);
    }

    /** Shows the popup. Will have no effect if the popup is already showing. */
    public void show() {
        if (mPopupWindow.isShowing()) return;

        if (mDismissOnScreenSizeChange) {
            mWindowBoundsChangeDetector = new WindowBoundsChangeDetector(mRootView, this::dismiss);
        }

        mRectProvider.startObserving(this);
        mViewportRectProvider.startObserving(this);

        updatePopupLayout();
        if (hasMinimalSize()) showPopupWindow();
    }

    /**
     * Disposes of the popup window. Will have no effect if the popup isn't showing.
     *
     * @see PopupWindow#dismiss()
     */
    public void dismiss() {
        if (mWindowBoundsChangeDetector != null) {
            mWindowBoundsChangeDetector.detach();
            mWindowBoundsChangeDetector = null;
        }
        mPopupWindow.dismiss();
    }

    /** Used for testing only. Explicitly trigger dismiss listeners. */
    public void onDismissForTesting(boolean byInsideTouch) {
        mBeingDismissedByTouch = byInsideTouch;
        mDismissedByInsideTouch = byInsideTouch;
        mDismissListener.onDismiss();
    }

    /**
     * @return Whether the popup is currently showing.
     */
    public boolean isShowing() {
        return mPopupWindow.isShowing();
    }

    /**
     * Sets the {@link LayoutObserver} for this AnchoredPopupWindow.
     *
     * @param layoutObserver The observer to be notified of layout changes.
     * @deprecated Use the {@link Builder} to set this value during construction.
     */
    @Deprecated
    public void setLayoutObserver(@Nullable LayoutObserver layoutObserver) {
        mLayoutObserver = layoutObserver;
    }

    /**
     * @param onTouchListener A callback for all touch events being dispatched to the popup.
     * @see PopupWindow#setTouchInterceptor(OnTouchListener)
     */
    public void setTouchInterceptor(@Nullable OnTouchListener onTouchListener) {
        mTouchListener = onTouchListener;
    }

    /**
     * @param onDismissListener A listener to be called when the popup is dismissed.
     * @see PopupWindow#setOnDismissListener(OnDismissListener)
     * @deprecated Use the {@link Builder} to set this value during construction.
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
     * @param dismiss Whether or not to dismiss this popup when the screen is tapped. This will
     *     happen for both taps inside and outside the popup except when a tap is handled by child
     *     views. The default is {@code false}.
     * @deprecated Use the {@link Builder} to set this value during construction.
     */
    @Deprecated
    public void setDismissOnTouchInteraction(boolean dismiss) {
        mDismissOnTouchInteraction = dismiss;
        mPopupWindow.setOutsideTouchable(mDismissOnTouchInteraction);
    }

    /**
     * @param dismiss Whether or not to dismiss this popup when the screen size changes.
     * @deprecated Use the {@link Builder} to set this value during construction.
     */
    @Deprecated
    public void setDismissOnScreenSizeChange(boolean dismiss) {
        mDismissOnScreenSizeChange = dismiss;
    }

    /**
     * If set to true, popup will be notified when an outside touch happens. It is not the
     * equivalent of closing the popup on all touch events. The user can still interact with the
     * popup by sending inside touch events. If set to false, the popup won't be notified about the
     * outside touch event.
     *
     * @param touchable Whether or not to notify the popup when an outside touch happens. The
     *     default is {@code false}.
     * @deprecated Use the {@link Builder} to set this value during construction.
     */
    @Deprecated
    public void setOutsideTouchable(boolean touchable) {
        mPopupWindow.setOutsideTouchable(touchable);
    }

    /**
     * Sets the layout type of this window.
     *
     * @param layoutType The layout type of the window.
     * @deprecated Use the {@link Builder} to set this value during construction.
     */
    @Deprecated
    private void setWindowLayoutType(int layoutType) {
        mPopupWindow.setWindowLayoutType(layoutType);
    }

    /**
     * Sets the preferred vertical orientation of the popup with respect to the anchor Rect such as
     * above or below the anchor. This should be called before the popup is shown.
     *
     * @param orientation The vertical orientation preferred.
     * @deprecated Use the {@link Builder} to set this value during construction.
     */
    @Deprecated
    public void setPreferredVerticalOrientation(@VerticalOrientation int orientation) {
        mPreferredVerticalOrientation = orientation;
    }

    /**
     * Sets the preferred horizontal orientation of the popup with respect to the anchor Rect such
     * as centered with respect to the anchor. This should be called before the popup is shown.
     *
     * @param orientation The horizontal orientation preferred.
     * @deprecated Use the {@link Builder} to set this value during construction.
     */
    @Deprecated
    public void setPreferredHorizontalOrientation(@HorizontalOrientation int orientation) {
        mPreferredHorizontalOrientation = orientation;
    }

    /**
     * Sets the animation style for the popup. This should be called before the popup is shown.
     * Setting this style will take precedence over {@link #setAnimateFromAnchor(boolean)}.
     *
     * @param animationStyleId The id of the animation style.
     * @deprecated Use the {@link Builder} to set this value during construction.
     */
    @Deprecated
    public void setAnimationStyle(int animationStyleId) {
        mAnimationStyleId = animationStyleId;
        mPopupWindow.setAnimationStyle(animationStyleId);
    }

    /**
     * Set whether the popup should enter from / exit to the anchor point. This should be called
     * before the popup is shown. If an animation style is specified by {@link
     * #setAnimationStyle(int)}, this method will have no effect.
     *
     * @param animateFromAnchor Whether the popup should animator from anchor point.
     * @deprecated Use the {@link Builder} to set this value during construction.
     */
    @Deprecated
    public void setAnimateFromAnchor(boolean animateFromAnchor) {
        mAnimateFromAnchor = animateFromAnchor;
    }

    /**
     * If set to true, orientation will be updated every time that the {@link OnRectChanged} is
     * called.
     *
     * @deprecated Use the {@link Builder} to set this value during construction.
     */
    @Deprecated
    public void setUpdateOrientationOnChange(boolean updateOrientationOnChange) {
        mUpdateOrientationOnChange = updateOrientationOnChange;
    }

    /**
     * Changes the focusability of the popup. See {@link PopupWindow#setFocusable(boolean)}.
     *
     * @param focusable True if the popup is focusable, false otherwise.
     * @deprecated Use the {@link Builder} to set this value during construction.
     */
    @Deprecated
    public void setFocusable(boolean focusable) {
        mPopupWindow.setFocusable(focusable);
    }

    /**
     * Changes whether the popup is touch modal or if outside touches will be sent to other windows
     * behind it. See {@link PopupWindow#setTouchModal(boolean)}.
     *
     * @param touchModal True to sent all outside touches to this window, false to other windows
     *     behind it.
     * @deprecated Use the {@link Builder} to set this value during construction.
     */
    @Deprecated
    public void setTouchModal(boolean touchModal) {
        mPopupWindow.setTouchModal(touchModal);
    }

    /**
     * Sets the margin for the popup window. This should be called before the popup is shown.
     *
     * @param margin The margin in pixels.
     * @deprecated Use the {@link Builder} to set this value during construction.
     */
    @Deprecated
    public void setMargin(int margin) {
        mMarginPx = margin;
    }

    /**
     * Sets the max width for the popup. This should be called before the popup is shown.
     *
     * @param maxWidth The max width for the popup.
     * @deprecated Use the {@link Builder} to set this value during construction.
     */
    @Deprecated
    public void setMaxWidth(int maxWidth) {
        final float density = mRootView.getResources().getDisplayMetrics().density;
        mMaxWidthPx = Math.max(maxWidth, (int) Math.ceil(density * MIN_TOUCHABLE_WIDTH_DIP));
    }

    /**
     * Sets whether the popup should horizontally overlap the anchor {@link Rect}. Defaults to
     * false. This should be called before the popup is shown.
     *
     * @param overlap Whether the popup should overlap the anchor.
     * @deprecated Use the {@link Builder} to set this value during construction.
     */
    @Deprecated
    public void setHorizontalOverlapAnchor(boolean overlap) {
        mHorizontalOverlapAnchor = overlap;
    }

    /**
     * Sets whether the popup should vertically overlap the anchor {@link Rect}. Defaults to false.
     * This should be called before the popup is shown.
     *
     * @param overlap Whether the popup should overlap the anchor.
     * @deprecated Use the {@link Builder} to set this value during construction.
     */
    @Deprecated
    public void setVerticalOverlapAnchor(boolean overlap) {
        mVerticalOverlapAnchor = overlap;
    }

    /**
     * Set whether we want position the context menu around the anchor to maximize its width.
     *
     * <p>If this is set, in some cases the popup will be shown differently than the settings for
     * {@link #setVerticalOverlapAnchor} and {@link #setHorizontalOverlapAnchor}. (e.g. Side of
     * anchor is too narrow to make it not horizontally smartAnchor).
     *
     * <p>This should be called before the popup is shown.
     *
     * @param smartAnchor Whether the popup should smartAnchor the anchor.
     * @deprecated Use the {@link Builder} to set this value during construction.
     */
    @Deprecated
    public void setSmartAnchorWithMaxWidth(boolean smartAnchor) {
        mSmartAnchorWithMaxWidth = smartAnchor;
    }

    /**
     * Sets the elevation of the popup.
     *
     * @deprecated Use the {@link Builder} to set this value during construction.
     */
    @Deprecated
    public void setElevation(float elevation) {
        mPopupWindow.setElevation(elevation);
    }

    /**
     * Sets the desired width for the content of the popup window.
     *
     * <p>You can call this method only before {@link #show()} as it does not trigger relayout,
     * whereas {@link #setDesiredContentSize(int, int)} triggers it.
     *
     * @deprecated Use the {@link Builder} to set this value during construction.
     */
    @Deprecated
    public void setDesiredContentWidth(int width) {
        updateDesiredContentSize(width, mDesiredContentHeight, false);
    }

    /**
     * Sets the desired dimensions for the content of the popup window.
     *
     * <p>Pass 0 to either dimension to have it determine its own size. The popup window will be
     * shown in this exact size unless certain constraint presents (e.g. desiredContentWidth >
     * maxWidthPx).
     *
     * <p>This method triggers an update of the layout if the popup is already shown. You can call
     * it to resize the popup at any time.
     *
     * @deprecated Use the {@link Builder} to set this value during construction.
     */
    @Deprecated
    public void setDesiredContentSize(int width, int height) {
        updateDesiredContentSize(width, height, true);
    }

    /**
     * Sets the desired dimensions for the content of the popup window.
     *
     * <p>Pass 0 to either dimension to have it determine its own size. The popup window will be
     * shown in this exact size unless certain constraint presents (e.g. desiredContentWidth >
     * maxWidthPx).
     *
     * <p>This method triggers an update of the layout if the popup is already shown. You can call
     * it to resize the popup at any time.
     *
     * @param height The desired height for the popup.
     * @param width The desired width for the popup.
     * @param updateLayout Whether trigger layout update
     */
    public void updateDesiredContentSize(int width, int height, boolean updateLayout) {
        mDesiredContentWidth = width;
        mDesiredContentHeight = height;
        if (updateLayout) updatePopupLayout();
    }

    /**
     * Sets whether to allow the popup to have a small non-touchable size. The default is false.
     *
     * @deprecated Use the {@link Builder} to set this value during construction.
     */
    @Deprecated
    public void setAllowNonTouchableSize(boolean allowNonTouchableSize) {
        mAllowNonTouchableSize = allowNonTouchableSize;
    }

    /** Changes the background at runtime. */
    public void setBackgroundDrawable(Drawable background) {
        mPopupWindow.setBackgroundDrawable(background);
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
            if (!touchInterceptedByChild) {
                mBeingDismissedByTouch = true;
                mDismissedByInsideTouch = event.getAction() != MotionEvent.ACTION_OUTSIDE;
                dismiss();
            }
        }

        return touchInterceptedByClient;
    }

    /**
     * Return if the popup was dismissed by inside touch last time. It shouldn't be called when the
     * popup is showing.
     */
    public boolean wasDismissedByInsideTouch() {
        assert !isShowing();
        return mDismissedByInsideTouch;
    }

    /**
     * Causes this popup to position/size itself. The calculations will happen even if the popup
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
        boolean currentPositionBelow = mPopupSpec.positionParams.isPositionBelow;
        boolean currentPositionToLeft = mPopupSpec.positionParams.isPositionToLeft;
        boolean preferCurrentOrientation = mPopupWindow.isShowing() && !mUpdateOrientationOnChange;

        mPopupSpec =
                mSpecCalculator.getPopupWindowSpec(
                        mViewportRectProvider.getRect(),
                        anchorRect,
                        getOrCreateContentView(),
                        mRootView.getWidth(),
                        paddingX,
                        paddingY,
                        mMarginPx,
                        mMaxWidthPx,
                        mDesiredContentWidth,
                        mDesiredContentHeight,
                        mPreferredHorizontalOrientation,
                        mPreferredVerticalOrientation,
                        currentPositionBelow,
                        currentPositionToLeft,
                        preferCurrentOrientation,
                        mHorizontalOverlapAnchor,
                        mVerticalOverlapAnchor,
                        mSmartAnchorWithMaxWidth);

        boolean isPositionBelow = mPopupSpec.positionParams.isPositionBelow;
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

    private View getOrCreateContentView() {
        if (mContentView == null) {
            mContentView = mContentViewCreator.get();
        }
        return mContentView;
    }

    /**
     * Gets the content view of the {@link PopupWindow}.
     *
     * @return The content view.
     */
    public @Nullable View getContentView() {
        return mContentView;
    }

    /**
     * Checks if the popup spec meets the minimal size requirements.
     *
     * <p>By default, this method ensures that the size is sufficient for users to see what they are
     * tapping. Popups can be very narrow (e.g. in landscape) and still be interactive. Use {@link
     * #setRequireTouchableSize(boolean)} to disable this check.
     *
     * @return True if the popup is large enough to be safely shown to users.
     */
    private boolean hasMinimalSize() {
        if (mAllowNonTouchableSize) {
            return true;
        }

        final float density = mRootView.getResources().getDisplayMetrics().density;
        return mPopupSpec.popupRect.height() >= density * MIN_TOUCHABLE_HEIGHT_DIP
                && mPopupSpec.popupRect.width() >= density * MIN_TOUCHABLE_WIDTH_DIP;
    }

    @VisibleForTesting
    void showPopupWindow() {
        if (mAnimateFromAnchor && mAnimationStyleId == 0) {
            int animationStyle =
                    calculateAnimationStyle(
                            mPopupSpec.positionParams.isPositionBelow,
                            mPopupSpec.positionParams.isPositionToLeft);
            mPopupWindow.setAnimationStyle(animationStyle);
        }
        try {
            assert hasMinimalSize();
            mPopupWindow.setContentView(getOrCreateContentView());
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

    /** An interface for customizing the positioning behavior. */
    public interface SpecCalculator {
        /**
         * Calculate the Rect where the popup window will displayed on the current application
         * window.
         *
         * @param freeSpaceRect The rect representing the window size. Always starts from (0,0) as
         *     top left.
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
         * @param preferCurrentOrientation Whether prefer to reserve the popup orientation. If this
         *     set to true, popup window will prefer to show below / to left of anchored window the
         *     same way as |currentPositionBelow| and |currentPositionToLeft|.
         * @param horizontalOverlapAnchor Value set by {@link #setHorizontalOverlapAnchor(boolean)}.
         * @param verticalOverlapAnchor Value set by {@link #setVerticalOverlapAnchor(boolean)}.
         * @param smartAnchorWithMaxWidth Value set by {@link #setSmartAnchorWithMaxWidth(boolean)}.
         * @return {@link PopupSpec} that includes the popup specs (e.g. location in window)
         */
        PopupSpec getPopupWindowSpec(
                Rect freeSpaceRect,
                Rect anchorRect,
                View contentView,
                int rootViewWidth,
                int paddingX,
                int paddingY,
                int marginPx,
                int maxWidthPx,
                int desiredContentWidth,
                int desiredContentHeight,
                @HorizontalOrientation int preferredHorizontalOrientation,
                @VerticalOrientation int preferredVerticalOrientation,
                boolean currentPositionBelow,
                boolean currentPositionToLeft,
                boolean preferCurrentOrientation,
                boolean horizontalOverlapAnchor,
                boolean verticalOverlapAnchor,
                boolean smartAnchorWithMaxWidth);
    }

    /**
     * A data structure that holds the parameters and constraints for calculating the position of a
     * popup window.
     */
    static class PopupPositionParams {

        /** The maximum width of the content of the popup window. */
        public final int maxContentWidth;

        /** The maximum height of the content of the popup window. */
        public final int maxContentHeight;

        /** Whether the popup window shows to the left of the anchored rect. */
        public final boolean isPositionToLeft;

        /** Whether the popup window shows below the anchored rect. */
        public final boolean isPositionBelow;

        /** Whether the popup window should overlap the anchor view horizontally. */
        public final boolean allowHorizontalOverlap;

        /** Whether the popup window should overlap the anchor view vertically. */
        public final boolean allowVerticalOverlap;

        PopupPositionParams(
                int maxContentWidth,
                int maxContentHeight,
                boolean isPositionToLeft,
                boolean isPositionBelow,
                boolean allowHorizontalOverlap,
                boolean allowVerticalOverlap) {
            this.maxContentWidth = maxContentWidth;
            this.maxContentHeight = maxContentHeight;
            this.isPositionToLeft = isPositionToLeft;
            this.isPositionBelow = isPositionBelow;
            this.allowHorizontalOverlap = allowHorizontalOverlap;
            this.allowVerticalOverlap = allowVerticalOverlap;
        }
    }

    /**
     * Helper class holds information of popup window (e.g. rect on screen, position to anchorRect)
     */
    @VisibleForTesting
    static class PopupSpec {
        /** Parameters and constraints for the popup window. */
        public final PopupPositionParams positionParams;

        /** Location of the popup window in the current application window. */
        public final Rect popupRect;

        PopupSpec(Rect rect, PopupPositionParams positionParams) {
            this.popupRect = rect;
            this.positionParams = positionParams;
        }
    }

    /**
     * Calculate the style Id to use when showing the popup window, when {@link
     * #setAnimateFromAnchor(true)}.
     *
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
}
