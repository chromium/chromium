// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.ViewParent;
import android.widget.FrameLayout;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.EventOffsetHandler;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

/**
 * TopControlsContainerView is responsible for holding the top-view from the client. Further, it
 * has a ViewResourceAdapter that is kept in sync with the contents of the top-view.
 * ViewResourceAdapter is used to keep a bitmap in sync with the contents of the top-view. The
 * bitmap is placed in a cc::Layer and the layer is shown while scrolling the top-view.
 * ViewResourceAdapter is always kept in sync, as to do otherwise results in a noticeable delay
 * between when the scroll starts the content is available.
 *
 * There are many parts involved in orchestrating top-controls scrolling. The key things to know
 * are:
 * . TopControlsContainerView (in native code) keeps a cc::Layer that shows a bitmap rendered by
 *   the top-view. The bitmap is updated anytime the top-view changes. This is done as otherwise
 *   there is a noticable delay between when the scroll starts and the bitmap is available.
 * . When scrolling, the cc::Layer for the WebContents and TopControlsContainerView is moved.
 * . The size of the WebContents is only changed after the user releases a touch point. Otherwise
 *   the scrollbar bounces around.
 * . WebContentsDelegate::DoBrowserControlsShrinkRendererSize() only changes when the WebContents
 *   size change.
 * . WebContentsGestureStateTracker is responsible for determining when a scroll/touch is underway.
 * . ContentViewRenderView.Delegate is used to adjust the size of the webcontents when the
 *   top-controls are fully visible (and a scroll is not underway).
 *
 * The flow of this code is roughly:
 * . WebContentsGestureStateTracker generally detects a touch first
 * . TabImpl is notified and caches state.
 * . onTopControlsChanged() is called. This triggers hiding the real view and calling to native code
 *   to move the cc::Layers.
 * . the move continues.
 * . when the move completes and both WebContentsGestureStateTracker and TopControlsContainerView
 *   no longer believe a move/gesture/scroll is underway the size of the WebContents is adjusted
 *   (if necessary).
 */
@JNINamespace("weblayer")
class TopControlsContainerView extends FrameLayout {
    // ID used with ViewResourceAdapter.
    private static final int TOP_CONTROLS_ID = 1001;

    private static final long SYSTEM_UI_VIEWPORT_UPDATE_DELAY_MS = 500;

    private long mNativeTopControlsContainerView;

    private ViewResourceAdapter mViewResourceAdapter;

    // Last width/height of mView as sent to the native side.
    private int mLastWidth;
    private int mLastHeight;

    // view from the client.
    private View mView;

    private ContentViewRenderView mContentViewRenderView;
    private WebContents mWebContents;
    private EventOffsetHandler mEventOffsetHandler;
    private int mTopContentOffset;

    // Set to true if |mView| is hidden because the user has scrolled or triggered some action such
    // that mView is not visible. While |mView| is not visible if this is true, the bitmap from
    // |mView| may be partially visible.
    private boolean mInTopControlsScroll;

    private boolean mIsFullscreen;

    // Used to delay processing fullscreen requests.
    private Runnable mSystemUiFullscreenResizeRunnable;

    private final Listener mListener;

    public interface Listener {
        /**
         * Called when the top-controls are either completely showing, or completely hiding.
         */
        public void onTopControlsCompletelyShownOrHidden();
    }

    // Used to  delay updating the image for the layer.
    private final Runnable mRefreshResourceIdRunnable = () -> {
        if (mView == null) return;
        TopControlsContainerViewJni.get().updateTopControlsResource(
                mNativeTopControlsContainerView, TopControlsContainerView.this);
    };

    TopControlsContainerView(
            Context context, ContentViewRenderView contentViewRenderView, Listener listener) {
        super(context);
        mContentViewRenderView = contentViewRenderView;
        mEventOffsetHandler =
                new EventOffsetHandler(new EventOffsetHandler.EventOffsetHandlerDelegate() {
                    @Override
                    public float getTop() {
                        return mTopContentOffset;
                    }

                    @Override
                    public void setCurrentTouchEventOffsets(float top) {
                        if (mWebContents != null) {
                            mWebContents.getEventForwarder().setCurrentTouchEventOffsets(0, top);
                        }
                    }
                });
        mNativeTopControlsContainerView =
                TopControlsContainerViewJni.get().createTopControlsContainerView(
                        this, contentViewRenderView.getNativeHandle());
        mListener = listener;
    }

    public void setWebContents(WebContents webContents) {
        mWebContents = webContents;
        TopControlsContainerViewJni.get().setWebContents(
                mNativeTopControlsContainerView, TopControlsContainerView.this, webContents);
    }

    public void destroy() {
        setView(null);
        TopControlsContainerViewJni.get().deleteTopControlsContainerView(
                mNativeTopControlsContainerView, TopControlsContainerView.this);
    }

    public long getNativeHandle() {
        return mNativeTopControlsContainerView;
    }

    public EventOffsetHandler getEventOffsetHandler() {
        return mEventOffsetHandler;
    }

    /**
     * Returns the vertical offset for the WebContents.
     */
    public int getTopContentOffset() {
        return mView == null ? 0 : mTopContentOffset;
    }

    /**
     * Returns true if the top control is visible to the user.
     */
    public boolean isTopControlVisible() {
        // Don't check the visibility of the View itself as it's hidden while scrolling.
        return mView != null && mTopContentOffset != 0;
    }

    /**
     * Sets the view from the client.
     */
    public void setView(View view) {
        if (mView == view) return;
        if (mView != null) {
            if (mView.getParent() == this) removeView(mView);
            // TODO: need some sort of destroy to drop reference.
            mViewResourceAdapter = null;
            TopControlsContainerViewJni.get().deleteTopControlsLayer(
                    mNativeTopControlsContainerView, TopControlsContainerView.this);
            mContentViewRenderView.getResourceManager()
                    .getDynamicResourceLoader()
                    .unregisterResource(TOP_CONTROLS_ID);
        }
        mView = view;
        if (mView == null) return;
        addView(view,
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT,
                        FrameLayout.LayoutParams.UNSPECIFIED_GRAVITY));
        if (getWidth() > 0 && getHeight() > 0) {
            view.layout(0, 0, getWidth(), getHeight());
            createAdapterAndLayer();
        }
        if (mIsFullscreen) hideTopControls();
    }

    public View getView() {
        return mView;
    }

    /**
     * Called from ViewAndroidDelegate, see it for details.
     */
    public void onTopControlsChanged(int topControlsOffsetY, int topContentOffsetY) {
        if (mView == null) return;
        if (mIsFullscreen) return;
        if (topContentOffsetY == getHeight()) {
            finishTopControlsScroll(topContentOffsetY);
            return;
        }
        if (!mInTopControlsScroll) prepareForTopControlsScroll();
        setTopControlsOffset(topControlsOffsetY, topContentOffsetY);
    }

    @SuppressLint("NewApi") // Used on O+, invalidateChildInParent used for previous versions.
    @Override
    public void onDescendantInvalidated(View child, View target) {
        super.onDescendantInvalidated(child, target);
        invalidateViewResourceAdapter();
    }

    @Override
    public ViewParent invalidateChildInParent(int[] location, Rect dirty) {
        invalidateViewResourceAdapter();
        return super.invalidateChildInParent(location, dirty);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        if (mView == null) return;
        int width = right - left;
        int height = bottom - top;
        if (height != mLastHeight || width != mLastWidth) {
            mLastWidth = width;
            mLastHeight = height;
            if (mLastWidth > 0 && mLastHeight > 0) {
                if (mViewResourceAdapter == null) {
                    createAdapterAndLayer();
                } else {
                    TopControlsContainerViewJni.get().setTopControlsSize(
                            mNativeTopControlsContainerView, TopControlsContainerView.this,
                            mLastWidth, mLastHeight);
                }
            }
        }
    }

    /**
     * Triggers copying the contents of mView to the offscreen buffer.
     */
    private void invalidateViewResourceAdapter() {
        if (mViewResourceAdapter == null || mView.getVisibility() != View.VISIBLE) return;
        mViewResourceAdapter.invalidate(null);
        removeCallbacks(mRefreshResourceIdRunnable);
        postOnAnimation(mRefreshResourceIdRunnable);
    }

    /**
     * Creates mViewResourceAdapter and the layer showing a copy of mView.
     */
    private void createAdapterAndLayer() {
        assert mViewResourceAdapter == null;
        assert mView != null;
        mViewResourceAdapter = new ViewResourceAdapter(mView);
        mContentViewRenderView.getResourceManager().getDynamicResourceLoader().registerResource(
                TOP_CONTROLS_ID, mViewResourceAdapter);
        // It's important that the layer is created immediately and always kept in sync with the
        // View. Creating the layer only when needed results in a noticeable delay between when
        // the layer is created and actually shown. Chrome for Android does the same thing.
        TopControlsContainerViewJni.get().createTopControlsLayer(
                mNativeTopControlsContainerView, TopControlsContainerView.this, TOP_CONTROLS_ID);
        mLastWidth = getWidth();
        mLastHeight = getHeight();
        TopControlsContainerViewJni.get().setTopControlsSize(mNativeTopControlsContainerView,
                TopControlsContainerView.this, mLastWidth, mLastHeight);
    }

    private void finishTopControlsScroll(int topContentOffsetY) {
        mInTopControlsScroll = false;
        setTopControlsOffset(0, topContentOffsetY);
        mContentViewRenderView.postOnAnimation(() -> showTopControls());
    }

    /**
     * Returns true if the top-controls are completely shown or completely hidden. A return value
     * of false indicates the top-controls are being moved.
     */
    public boolean isTopControlsCompletelyShownOrHidden() {
        return mTopContentOffset == 0 || mTopContentOffset == getHeight();
    }

    private void setTopControlsOffset(int topControlsOffsetY, int topContentOffsetY) {
        mTopContentOffset = topContentOffsetY;
        if (isTopControlsCompletelyShownOrHidden()) {
            mListener.onTopControlsCompletelyShownOrHidden();
        }
        TopControlsContainerViewJni.get().setTopControlsOffset(mNativeTopControlsContainerView,
                TopControlsContainerView.this, topControlsOffsetY, topContentOffsetY);
    }

    private void prepareForTopControlsScroll() {
        mInTopControlsScroll = true;
        mContentViewRenderView.postOnAnimation(() -> hideTopControls());
    }

    private void hideTopControls() {
        if (mView != null) mView.setVisibility(View.INVISIBLE);
    }

    private void showTopControls() {
        if (mView != null) mView.setVisibility(View.VISIBLE);
    }

    @CalledByNative
    private void didToggleFullscreenModeForTab(final boolean isFullscreen) {
        // Delay hiding until after the animation. This comes from Chrome code.
        if (mSystemUiFullscreenResizeRunnable != null) {
            getHandler().removeCallbacks(mSystemUiFullscreenResizeRunnable);
        }
        mSystemUiFullscreenResizeRunnable = () -> processFullscreenChanged(isFullscreen);
        long delay = isFullscreen ? SYSTEM_UI_VIEWPORT_UPDATE_DELAY_MS : 0;
        postDelayed(mSystemUiFullscreenResizeRunnable, delay);
    }

    private void processFullscreenChanged(boolean isFullscreen) {
        mSystemUiFullscreenResizeRunnable = null;
        if (mIsFullscreen == isFullscreen) return;
        mIsFullscreen = isFullscreen;
        if (mView == null) return;
        if (mIsFullscreen) {
            hideTopControls();
            setTopControlsOffset(-mLastHeight, 0);
        } else {
            showTopControls();
            setTopControlsOffset(0, mLastHeight);
        }
    }

    @NativeMethods
    interface Natives {
        long createTopControlsContainerView(
                TopControlsContainerView view, long nativeContentViewRenderView);
        void deleteTopControlsContainerView(
                long nativeTopControlsContainerView, TopControlsContainerView caller);
        void createTopControlsLayer(
                long nativeTopControlsContainerView, TopControlsContainerView caller, int id);
        void deleteTopControlsLayer(
                long nativeTopControlsContainerView, TopControlsContainerView caller);
        void setTopControlsOffset(long nativeTopControlsContainerView,
                TopControlsContainerView caller, int topControlsOffsetY, int topContentOffsetY);
        void setTopControlsSize(long nativeTopControlsContainerView,
                TopControlsContainerView caller, int width, int height);
        void updateTopControlsResource(
                long nativeTopControlsContainerView, TopControlsContainerView caller);
        void setWebContents(long nativeTopControlsContainerView, TopControlsContainerView caller,
                WebContents webContents);
    }
}
