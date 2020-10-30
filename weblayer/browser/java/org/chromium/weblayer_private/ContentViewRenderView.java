// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.SurfaceTexture;
import android.os.SystemClock;
import android.util.Size;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.TextureView;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.webkit.ValueCallback;
import android.widget.FrameLayout;
import android.widget.RelativeLayout;

import androidx.annotation.IntDef;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.components.browser_ui.widget.InsetObserverView;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.resources.ResourceManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;

/**
 * This class manages the chromium compositor and the Surface that is used by
 * the chromium compositor. Note it can be used to display only one WebContents.
 * This allows switching between SurfaceView and TextureView as the source of
 * the Surface used by chromium compositor, and attempts to make the switch
 * visually seamless.
 */
@JNINamespace("weblayer")
public class ContentViewRenderView
        extends RelativeLayout implements WindowAndroid.SelectionHandlesObserver {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({MODE_SURFACE_VIEW, MODE_TEXTURE_VIEW})
    public @interface Mode {}
    public static final int MODE_SURFACE_VIEW = 0;
    public static final int MODE_TEXTURE_VIEW = 1;

    private static final int CONFIG_TIMEOUT_MS = 1000;

    // A child view of this class. Parent of SurfaceView/TextureView.
    // Needed to support not resizing the surface when soft keyboard is showing.
    private final SurfaceParent mSurfaceParent;

    // This is mode that is requested by client.
    private SurfaceData mRequested;
    // This is the mode that last supplied the Surface to the compositor.
    // This should generally be equal to |mRequested| except during transitions.
    private SurfaceData mCurrent;

    // The native side of this object.
    private long mNativeContentViewRenderView;

    // An invisible view that notifies observers of changes to window insets and safe area.
    private InsetObserverView mInsetObserverView;

    private WindowAndroid mWindowAndroid;
    private WebContents mWebContents;

    private int mBackgroundColor;

    // This is the size of the surfaces, so the "physical" size for the compositor.
    // This is the size of the |mSurfaceParent| view, which is the immediate parent
    // of the SurfaceView/TextureView. Note this does not always match the size of
    // this ContentViewRenderView; when the soft keyboard is displayed,
    // ContentViewRenderView will shrink in height, but |mSurfaceParent| will not.
    private int mPhysicalWidth;
    private int mPhysicalHeight;

    private int mWebContentsHeightDelta;

    private boolean mCompositorHasSurface;

    private DisplayAndroid.DisplayAndroidObserver mDisplayAndroidObserver;

    private boolean mSelectionHandlesActive;

    // The time stamp when a configuration was detected (if any).
    // This is used along with a timeout to determine if a resize surface resize
    // is due to screen rotation.
    private long mConfigurationChangedTimestamp;

    // Common interface to listen to surface related events.
    private interface SurfaceEventListener {
        void surfaceCreated();
        void surfaceChanged(Surface surface, boolean canBeUsedWithSurfaceControl, int format,
                int width, int height);
        // |cacheBackBuffer| will delay destroying the EGLSurface until after the next swap.
        void surfaceDestroyed(boolean cacheBackBuffer);
        void surfaceRedrawNeededAsync(Runnable drawingFinished);
    }

    private final ArrayList<TrackedRunnable> mPendingRunnables = new ArrayList<>();

    // Runnables posted via View.postOnAnimation may not run after the view is detached,
    // if nothing else causes animation. However a pending runnable may held by a GC root
    // from the thread itself, and thus can cause leaks. This class here is so ensure that
    // on destroy, all pending tasks are run immediately so they do not lead to leaks.
    private abstract class TrackedRunnable implements Runnable {
        private boolean mHasRun;
        public TrackedRunnable() {
            mPendingRunnables.add(this);
        }

        @Override
        public final void run() {
            // View.removeCallbacks is not always reliable, and may run the callback even
            // after it has been removed.
            if (mHasRun) return;
            assert mPendingRunnables.contains(this);
            mPendingRunnables.remove(this);
            mHasRun = true;
            doRun();
        }

        protected abstract void doRun();
    }

    // Non-static implementation of SurfaceEventListener that forward calls to native Compositor.
    // It is also responsible for updating |mRequested| and |mCurrent|.
    private class SurfaceEventListenerImpl implements SurfaceEventListener {
        private SurfaceData mSurfaceData;

        public void setRequestData(SurfaceData surfaceData) {
            assert mSurfaceData == null;
            mSurfaceData = surfaceData;
        }

        @Override
        public void surfaceCreated() {
            assert mNativeContentViewRenderView != 0;
            assert mSurfaceData == ContentViewRenderView.this.mRequested
                    || mSurfaceData == ContentViewRenderView.this.mCurrent;
            if (ContentViewRenderView.this.mCurrent != null
                    && ContentViewRenderView.this.mCurrent != mSurfaceData) {
                ContentViewRenderView.this.mCurrent.markForDestroy(true /* hasNextSurface */);
                mSurfaceData.setSurfaceDataNeedsDestroy(ContentViewRenderView.this.mCurrent);
            }
            ContentViewRenderView.this.mCurrent = mSurfaceData;
            ContentViewRenderViewJni.get().surfaceCreated(mNativeContentViewRenderView);
        }

        @Override
        public void surfaceChanged(Surface surface, boolean canBeUsedWithSurfaceControl, int format,
                int width, int height) {
            assert mNativeContentViewRenderView != 0;
            assert mSurfaceData == ContentViewRenderView.this.mCurrent;
            ContentViewRenderViewJni.get().surfaceChanged(mNativeContentViewRenderView,
                    canBeUsedWithSurfaceControl, format, width, height, surface);
            mCompositorHasSurface = surface != null;
            maybeUpdatePhysicalBackingSize(width, height);
        }

        @Override
        public void surfaceDestroyed(boolean cacheBackBuffer) {
            assert mNativeContentViewRenderView != 0;
            assert mSurfaceData == ContentViewRenderView.this.mCurrent;
            ContentViewRenderViewJni.get().surfaceDestroyed(
                    mNativeContentViewRenderView, cacheBackBuffer);
            mCompositorHasSurface = false;
        }

        @Override
        public void surfaceRedrawNeededAsync(Runnable drawingFinished) {
            assert false; // NOTREACHED.
        }
    }

    // Abstract differences between SurfaceView and TextureView behind this class.
    // Also responsible for holding and calling callbacks.
    private class SurfaceData implements SurfaceEventListener {
        private class TextureViewWithInvalidate extends TextureView {
            public TextureViewWithInvalidate(Context context) {
                super(context);
            }

            @Override
            public void invalidate() {
                // TextureView is invalidated when it receives a new frame from its SurfaceTexture.
                // This is a safe place to indicate that this TextureView now has content and is
                // ready to be shown.
                super.invalidate();
                destroyPreviousData();
            }
        }

        @Mode
        private final int mMode;
        private final SurfaceEventListener mListener;
        private final FrameLayout mParent;
        private final boolean mAllowSurfaceControl;
        private final Runnable mEvict;

        private boolean mRanCallbacks;
        private boolean mMarkedForDestroy;
        private boolean mCachedSurfaceNeedsEviction;

        private boolean mNeedsOnSurfaceDestroyed;

        // During transitioning between two SurfaceData, there is a complicated series of calls to
        // avoid visual artifacts.
        // 1) Allocate new SurfaceData, and insert it into view hierarchy below the existing
        //    SurfaceData, so it is not yet showing.
        // 2) When Surface is allocated by new View, swap chromium compositor to the
        //    new Surface. |markForDestroy| is called on the previous SurfaceData, and the two
        //    SurfaceDatas are linked through these two variables.
        //    Note at this point the existing view is still visible.
        // 3) Wait until new SurfaceData decides that it has content and is ready to be shown
        //    * For TextureView, wait until TextureView.invalidate is called
        //    * For SurfaceView, wait for two swaps from the chromium compositor
        // 4) New SurfaceData calls |destroy| on previous SurfaceData.
        //    * For TextureView, it is simply detached immediately from the view tree
        //    * For SurfaceView, to avoid flicker, move it to the back first before and wait
        //      two frames before detaching.
        // 5) Previous SurfaceData runs callbacks on the new SurfaceData to signal the completion
        //    of the transition.
        private SurfaceData mPrevSurfaceDataNeedsDestroy;
        private SurfaceData mNextSurfaceDataNeedsRunCallback;

        private final SurfaceHolderCallback mSurfaceCallback;
        private final SurfaceView mSurfaceView;
        private int mNumSurfaceViewSwapsUntilVisible;

        private final TextureView mTextureView;
        private final TextureViewSurfaceTextureListener mSurfaceTextureListener;

        private final ArrayList<ValueCallback<Boolean>> mModeCallbacks = new ArrayList<>();
        private ArrayList<Runnable> mSurfaceRedrawNeededCallbacks;

        public SurfaceData(@Mode int mode, FrameLayout parent, SurfaceEventListener listener,
                int backgroundColor, boolean allowSurfaceControl, Runnable evict) {
            mMode = mode;
            mListener = listener;
            mParent = parent;
            mAllowSurfaceControl = allowSurfaceControl;
            mEvict = evict;
            if (mode == MODE_SURFACE_VIEW) {
                mSurfaceView = new SurfaceView(parent.getContext());
                mSurfaceView.setZOrderMediaOverlay(true);
                mSurfaceView.setBackgroundColor(backgroundColor);

                mSurfaceCallback = new SurfaceHolderCallback(this);
                mSurfaceView.getHolder().addCallback(mSurfaceCallback);
                mSurfaceView.setVisibility(View.VISIBLE);

                // TODO(boliu): This is only needed when video is lifted into a separate surface.
                // Keeping this constantly will use one more byte per pixel constantly.
                mSurfaceView.getHolder().setFormat(PixelFormat.TRANSLUCENT);

                mTextureView = null;
                mSurfaceTextureListener = null;
            } else if (mode == MODE_TEXTURE_VIEW) {
                mTextureView = new TextureViewWithInvalidate(parent.getContext());
                mSurfaceTextureListener = new TextureViewSurfaceTextureListener(this);
                mTextureView.setSurfaceTextureListener(mSurfaceTextureListener);
                mTextureView.setVisibility(VISIBLE);

                mSurfaceView = null;
                mSurfaceCallback = null;
            } else {
                throw new RuntimeException("Illegal mode: " + mode);
            }

            // This postOnAnimation is to avoid manipulating the view tree inside layout or draw.
            parent.postOnAnimation(new TrackedRunnable() {
                @Override
                protected void doRun() {
                    if (mMarkedForDestroy) return;
                    View view = (mMode == MODE_SURFACE_VIEW) ? mSurfaceView : mTextureView;
                    assert view != null;
                    // Always insert view for new surface below the existing view to avoid artifacts
                    // during surface swaps. Index 0 is the lowest child.
                    mParent.addView(view, 0,
                            new FrameLayout.LayoutParams(FrameLayout.LayoutParams.MATCH_PARENT,
                                    FrameLayout.LayoutParams.MATCH_PARENT));
                    mParent.invalidate();
                }
            });
        }

        public void setSurfaceDataNeedsDestroy(SurfaceData surfaceData) {
            assert !mMarkedForDestroy;
            assert mPrevSurfaceDataNeedsDestroy == null;
            mPrevSurfaceDataNeedsDestroy = surfaceData;
            mPrevSurfaceDataNeedsDestroy.mNextSurfaceDataNeedsRunCallback = this;
        }

        public @Mode int getMode() {
            return mMode;
        }

        public boolean getAllowSurfaceControl() {
            return mAllowSurfaceControl;
        }

        public void addCallback(ValueCallback<Boolean> callback) {
            assert !mMarkedForDestroy;
            mModeCallbacks.add(callback);
            if (mRanCallbacks) runCallbacks();
        }

        // Tearing down is separated into markForDestroy and destroy. After markForDestroy
        // this class will is guaranteed to not issue any calls to its SurfaceEventListener.
        public void markForDestroy(boolean hasNextSurface) {
            if (mMarkedForDestroy) return;
            mMarkedForDestroy = true;

            if (mNeedsOnSurfaceDestroyed) {
                // SurfaceView being used with SurfaceControl need to cache the back buffer
                // (EGLSurface). Otherwise the surface is destroyed immediate before the
                // SurfaceView is detached.
                mCachedSurfaceNeedsEviction = hasNextSurface && mMode == MODE_SURFACE_VIEW;
                mListener.surfaceDestroyed(mCachedSurfaceNeedsEviction);
                mNeedsOnSurfaceDestroyed = false;
            }
            runSurfaceRedrawNeededCallbacks();

            if (mMode == MODE_SURFACE_VIEW) {
                mSurfaceView.getHolder().removeCallback(mSurfaceCallback);
            } else if (mMode == MODE_TEXTURE_VIEW) {
                mTextureView.setSurfaceTextureListener(null);
            } else {
                assert false;
            }
        }

        // Remove view from parent hierarchy.
        public void destroy() {
            assert mMarkedForDestroy;
            runCallbacks();
            // This postOnAnimation is to avoid manipulating the view tree inside layout or draw.
            mParent.postOnAnimation(new TrackedRunnable() {
                @Override
                protected void doRun() {
                    if (mMode == MODE_SURFACE_VIEW) {
                        // Detaching a SurfaceView causes a flicker because the SurfaceView tears
                        // down the Surface in SurfaceFlinger before removing its hole in the view
                        // tree. This is a complicated heuristics to avoid this. It first moves the
                        // SurfaceView behind the new View. Then wait two frames before detaching
                        // the SurfaceView. Waiting for a single frame still causes flickers on
                        // high end devices like Pixel 3.
                        moveChildToBackWithoutDetach(mParent, mSurfaceView);
                        TrackedRunnable inner = new TrackedRunnable() {
                            @Override
                            public void doRun() {
                                mParent.removeView(mSurfaceView);
                                mParent.invalidate();
                                if (mCachedSurfaceNeedsEviction) {
                                    mEvict.run();
                                    mCachedSurfaceNeedsEviction = false;
                                }
                                runCallbackOnNextSurfaceData();
                            }
                        };
                        TrackedRunnable outer = new TrackedRunnable() {
                            @Override
                            public void doRun() {
                                mParent.postOnAnimation(inner);
                            }
                        };
                        mParent.postOnAnimation(outer);
                    } else if (mMode == MODE_TEXTURE_VIEW) {
                        mParent.removeView(mTextureView);
                        runCallbackOnNextSurfaceData();
                    } else {
                        assert false;
                    }
                }
            });
        }

        private void moveChildToBackWithoutDetach(ViewGroup parent, View child) {
            final int numberOfChildren = parent.getChildCount();
            final int childIndex = parent.indexOfChild(child);
            if (childIndex <= 0) return;
            for (int i = 0; i < childIndex; ++i) {
                parent.bringChildToFront(parent.getChildAt(0));
            }
            assert parent.indexOfChild(child) == 0;
            for (int i = 0; i < numberOfChildren - childIndex - 1; ++i) {
                parent.bringChildToFront(parent.getChildAt(1));
            }
            parent.invalidate();
        }

        public void setBackgroundColor(int color) {
            assert !mMarkedForDestroy;
            if (mMode == MODE_SURFACE_VIEW) {
                mSurfaceView.setBackgroundColor(color);
            }
        }

        /** @return true if should keep swapping frames */
        public boolean didSwapFrame() {
            if (mSurfaceView != null && mSurfaceView.getBackground() != null) {
                mSurfaceView.post(new Runnable() {
                    @Override
                    public void run() {
                        if (mSurfaceView != null) mSurfaceView.setBackgroundResource(0);
                    }
                });
            }
            if (mMode == MODE_SURFACE_VIEW) {
                // We have no reliable signal for when to show a SurfaceView. This is a heuristic
                // (used by chrome as well) is to wait for 2 swaps from the chromium comopsitor
                // as a signal that the SurfaceView has content and is ready to be displayed.
                if (mNumSurfaceViewSwapsUntilVisible > 0) {
                    mNumSurfaceViewSwapsUntilVisible--;
                }
                if (mNumSurfaceViewSwapsUntilVisible == 0) {
                    destroyPreviousData();
                }
                return mNumSurfaceViewSwapsUntilVisible > 0;
            }
            return false;
        }

        public void runSurfaceRedrawNeededCallbacks() {
            ArrayList<Runnable> callbacks = mSurfaceRedrawNeededCallbacks;
            mSurfaceRedrawNeededCallbacks = null;
            if (callbacks == null) return;
            for (Runnable r : callbacks) {
                r.run();
            }
        }

        public View getView() {
            return mMode == MODE_SURFACE_VIEW ? mSurfaceView : mTextureView;
        }

        private void destroyPreviousData() {
            if (mPrevSurfaceDataNeedsDestroy != null) {
                mPrevSurfaceDataNeedsDestroy.destroy();
                mPrevSurfaceDataNeedsDestroy = null;
            }
        }

        @Override
        public void surfaceCreated() {
            if (mMarkedForDestroy) return;

            // On pre-M Android, layers start in the hidden state until a relayout happens.
            // There is a bug that manifests itself when entering overlay mode on pre-M devices,
            // where a relayout never happens. This bug is out of Chromium's control, but can be
            // worked around by forcibly re-setting the visibility of the surface view.
            // Otherwise, the screen stays black, and some tests fail.
            if (mSurfaceView != null) {
                mSurfaceView.setVisibility(mSurfaceView.getVisibility());
            }
            mListener.surfaceCreated();

            if (!mRanCallbacks && mPrevSurfaceDataNeedsDestroy == null) {
                runCallbacks();
            }

            mNeedsOnSurfaceDestroyed = true;
        }

        @Override
        public void surfaceChanged(Surface surface, boolean canBeUsedWithSurfaceControl, int format,
                int width, int height) {
            if (mMarkedForDestroy) return;
            // Selection magnifier does not work with surface control enabled.
            mListener.surfaceChanged(surface, canBeUsedWithSurfaceControl && mAllowSurfaceControl,
                    format, width, height);
            mNumSurfaceViewSwapsUntilVisible = 2;
        }

        @Override
        public void surfaceDestroyed(boolean cacheBackBuffer) {
            if (mMarkedForDestroy) return;
            assert mNeedsOnSurfaceDestroyed;
            mListener.surfaceDestroyed(cacheBackBuffer);
            mNeedsOnSurfaceDestroyed = false;
            runSurfaceRedrawNeededCallbacks();
        }

        @Override
        public void surfaceRedrawNeededAsync(Runnable drawingFinished) {
            if (mMarkedForDestroy) {
                drawingFinished.run();
                return;
            }
            assert mNativeContentViewRenderView != 0;
            assert this == ContentViewRenderView.this.mCurrent;
            if (mSurfaceRedrawNeededCallbacks == null) {
                mSurfaceRedrawNeededCallbacks = new ArrayList<>();
            }
            mSurfaceRedrawNeededCallbacks.add(drawingFinished);
            ContentViewRenderViewJni.get().setNeedsRedraw(mNativeContentViewRenderView);
        }

        private void runCallbacks() {
            mRanCallbacks = true;
            if (mModeCallbacks.isEmpty()) return;
            // PostTask to avoid possible reentrancy problems with embedder code.
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
                ArrayList<ValueCallback<Boolean>> clone =
                        (ArrayList<ValueCallback<Boolean>>) mModeCallbacks.clone();
                mModeCallbacks.clear();
                for (ValueCallback<Boolean> run : clone) {
                    run.onReceiveValue(!mMarkedForDestroy);
                }
            });
        }

        private void runCallbackOnNextSurfaceData() {
            if (mNextSurfaceDataNeedsRunCallback != null) {
                mNextSurfaceDataNeedsRunCallback.runCallbacks();
                mNextSurfaceDataNeedsRunCallback = null;
            }
        }
    }

    // Adapter for SurfaceHoolder.Callback.
    private static class SurfaceHolderCallback implements SurfaceHolder.Callback2 {
        private final SurfaceEventListener mListener;

        public SurfaceHolderCallback(SurfaceEventListener listener) {
            mListener = listener;
        }

        @Override
        public void surfaceCreated(SurfaceHolder holder) {
            mListener.surfaceCreated();
        }

        @Override
        public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
            mListener.surfaceChanged(holder.getSurface(), true, format, width, height);
        }

        @Override
        public void surfaceDestroyed(SurfaceHolder holder) {
            mListener.surfaceDestroyed(false /* cacheBackBuffer */);
        }

        @Override
        public void surfaceRedrawNeeded(SurfaceHolder holder) {
            // Intentionally not implemented.
        }

        @Override
        public void surfaceRedrawNeededAsync(SurfaceHolder holder, Runnable drawingFinished) {
            mListener.surfaceRedrawNeededAsync(drawingFinished);
        }
    }

    // Adapter for TextureView.SurfaceTextureListener.
    private static class TextureViewSurfaceTextureListener
            implements TextureView.SurfaceTextureListener {
        private final SurfaceEventListener mListener;

        private SurfaceTexture mCurrentSurfaceTexture;
        private Surface mCurrentSurface;

        public TextureViewSurfaceTextureListener(SurfaceEventListener listener) {
            mListener = listener;
        }

        @Override
        public void onSurfaceTextureAvailable(
                SurfaceTexture surfaceTexture, int width, int height) {
            mListener.surfaceCreated();
            onSurfaceTextureSizeChanged(surfaceTexture, width, height);
        }

        @Override
        public boolean onSurfaceTextureDestroyed(SurfaceTexture surfaceTexture) {
            mListener.surfaceDestroyed(false /* cacheBackBuffer */);
            return true;
        }

        @Override
        public void onSurfaceTextureSizeChanged(
                SurfaceTexture surfaceTexture, int width, int height) {
            if (mCurrentSurfaceTexture != surfaceTexture) {
                mCurrentSurfaceTexture = surfaceTexture;
                mCurrentSurface = new Surface(mCurrentSurfaceTexture);
            }
            mListener.surfaceChanged(mCurrentSurface, false, PixelFormat.OPAQUE, width, height);
        }

        @Override
        public void onSurfaceTextureUpdated(SurfaceTexture surfaceTexture) {}
    }

    // This is a child of ContentViewRenderView and parent of SurfaceView/TextureView.
    // This exists to avoid resizing SurfaceView/TextureView when the soft keyboard is displayed.
    private class SurfaceParent extends FrameLayout {
        public SurfaceParent(Context context) {
            super(context);
        }

        @Override
        protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
            int existingHeight = getMeasuredHeight();
            // If width is the same and height shrinks, then check if we should
            // avoid this resize for displaying the soft keyboard.
            if (getMeasuredWidth() == MeasureSpec.getSize(widthMeasureSpec)
                    && existingHeight > MeasureSpec.getSize(heightMeasureSpec)
                    && shouldAvoidSurfaceResizeForSoftKeyboard()) {
                // Just set the height to the current height.
                heightMeasureSpec =
                        MeasureSpec.makeMeasureSpec(existingHeight, MeasureSpec.EXACTLY);
            }
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }

        @Override
        protected void onSizeChanged(int w, int h, int oldw, int oldh) {
            mPhysicalWidth = w;
            mPhysicalHeight = h;
        }
    }

    /**
     * Constructs a new ContentViewRenderView.
     * This should be called and the {@link ContentViewRenderView} should be added to the view
     * hierarchy before the first draw to avoid a black flash that is seen every time a
     * {@link SurfaceView} is added.
     * @param context The context used to create this.
     * @param recreateForConfigurationChange indicates that views are recreated after BrowserImpl
     *                                       is retained, but Activity is recreated, for a
     *                                       configuration change.
     */
    public ContentViewRenderView(Context context, boolean recreateForConfigurationChange) {
        super(context);
        mSurfaceParent = new SurfaceParent(context);
        addView(mSurfaceParent,
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.WRAP_CONTENT));

        mInsetObserverView = InsetObserverView.create(context);
        addView(mInsetObserverView);
        mInsetObserverView.addObserver(new InsetObserverView.WindowInsetObserver() {
            @Override
            public void onInsetChanged(int left, int top, int right, int bottom) {
                if (mWebContents != null && mWebContents.isFullscreenForCurrentTab()) {
                    updateWebContentsSize();
                }
            }

            @Override
            public void onSafeAreaChanged(Rect area) {}
        });
        if (recreateForConfigurationChange) updateConfigChangeTimeStamp();
    }

    /**
     * Initialization that requires native libraries should be done here.
     * Native code should add/remove the layers to be rendered through the ContentViewLayerRenderer.
     * @param rootWindow The {@link WindowAndroid} this render view should be linked to.
     */
    public void onNativeLibraryLoaded(WindowAndroid rootWindow, @Mode int mode) {
        assert rootWindow != null;
        mNativeContentViewRenderView =
                ContentViewRenderViewJni.get().init(ContentViewRenderView.this, rootWindow);
        assert mNativeContentViewRenderView != 0;
        mWindowAndroid = rootWindow;
        requestMode(mode, (Boolean result) -> {});
        mDisplayAndroidObserver = new DisplayAndroid.DisplayAndroidObserver() {
            @Override
            public void onRotationChanged(int rotation) {
                updateConfigChangeTimeStamp();
            }
        };
        mWindowAndroid.getDisplay().addObserver(mDisplayAndroidObserver);
        mWindowAndroid.addSelectionHandlesObserver(this);
        updateBackgroundColor();
    }

    public void requestMode(@Mode int mode, ValueCallback<Boolean> callback) {
        boolean allowSurfaceControl = !mSelectionHandlesActive;
        assert mode == MODE_SURFACE_VIEW || mode == MODE_TEXTURE_VIEW;
        assert callback != null;
        if (mRequested != null
                && (mRequested.getMode() != mode
                        || mRequested.getAllowSurfaceControl() != allowSurfaceControl)) {
            if (mRequested != mCurrent) {
                mRequested.markForDestroy(false /* hasNextSurface */);
                mRequested.destroy();
            }
            mRequested = null;
        }

        if (mRequested == null) {
            SurfaceEventListenerImpl listener = new SurfaceEventListenerImpl();
            mRequested = new SurfaceData(mode, mSurfaceParent, listener, mBackgroundColor,
                    allowSurfaceControl, this::evictCachedSurface);
            listener.setRequestData(mRequested);
        }
        assert mRequested.getMode() == mode;
        mRequested.addCallback(callback);
    }

    /**
     * Sets how much to decrease the height of the WebContents by.
     */
    public void setWebContentsHeightDelta(int delta) {
        if (delta == mWebContentsHeightDelta) return;
        mWebContentsHeightDelta = delta;
        updateWebContentsSize();
    }

    /**
     * Return the view used for selection magnifier readback.
     */
    public View getViewForMagnifierReadback() {
        if (mCurrent == null) return null;
        return mCurrent.getView();
    }

    private void updateWebContentsSize() {
        if (mWebContents == null) return;
        Size size = getViewportSize();
        mWebContents.setSize(size.getWidth(), size.getHeight() - mWebContentsHeightDelta);
    }

    /** {@link CompositorViewHolder#getViewportSize()} for explanation. */
    private Size getViewportSize() {
        if (mWebContents.isFullscreenForCurrentTab()
                && mWindowAndroid.getKeyboardDelegate().isKeyboardShowing(getContext(), this)) {
            Rect visibleRect = new Rect();
            getWindowVisibleDisplayFrame(visibleRect);
            return new Size(Math.min(visibleRect.width(), getWidth()),
                    Math.min(visibleRect.height(), getHeight()));
        }

        return new Size(getWidth(), getHeight());
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        updateWebContentsSize();
    }

    /**
     * View's method override to notify WindowAndroid about changes in its visibility.
     */
    @Override
    protected void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);

        if (mWindowAndroid == null) return;

        if (visibility == View.GONE) {
            mWindowAndroid.onVisibilityChanged(false);
        } else if (visibility == View.VISIBLE) {
            mWindowAndroid.onVisibilityChanged(true);
        }
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        updateBackgroundColor();
    }

    /**
     * Sets the background color of the surface / texture view.  This method is necessary because
     * the background color of ContentViewRenderView itself is covered by the background of
     * SurfaceView.
     * @param color The color of the background.
     */
    @Override
    public void setBackgroundColor(int color) {
        super.setBackgroundColor(color);
        mBackgroundColor = color;
        if (mRequested != null) {
            mRequested.setBackgroundColor(color);
        }
        if (mCurrent != null) {
            mCurrent.setBackgroundColor(color);
        }
        ContentViewRenderViewJni.get().updateBackgroundColor(mNativeContentViewRenderView);
    }

    // SelectionHandlesObserver overrides
    @Override
    public void onSelectionHandlesStateChanged(boolean active) {
        if (mSelectionHandlesActive == active) return;
        mSelectionHandlesActive = active;
        if (mCurrent == null) return;
        if (mCurrent.getMode() == MODE_TEXTURE_VIEW) return;

        // requestMode will take into account the updated |mSelectionHandlesActive|
        // and respond appropriately, even if mode is the same.
        requestMode(mCurrent.getMode(), (Boolean result) -> {});
    }

    public InsetObserverView getInsetObserverView() {
        return mInsetObserverView;
    }

    /**
     * Should be called when the ContentViewRenderView is not needed anymore so its associated
     * native resource can be freed.
     */
    public void destroy() {
        if (mRequested != null) {
            mRequested.markForDestroy(false /* hasNextSurface */);
            mRequested.destroy();
            if (mCurrent != null && mCurrent != mRequested) {
                mCurrent.markForDestroy(false /* hasNextSurface */);
                mCurrent.destroy();
            }
        }
        mRequested = null;
        mCurrent = null;

        if (mDisplayAndroidObserver != null) {
            mWindowAndroid.getDisplay().removeObserver(mDisplayAndroidObserver);
            mDisplayAndroidObserver = null;
        }
        mWindowAndroid.removeSelectionHandlesObserver(this);
        mWindowAndroid = null;

        while (!mPendingRunnables.isEmpty()) {
            TrackedRunnable runnable = mPendingRunnables.get(0);
            removeCallbacks(runnable);
            runnable.run();
            assert !mPendingRunnables.contains(runnable);
        }
        ContentViewRenderViewJni.get().destroy(mNativeContentViewRenderView);
        mNativeContentViewRenderView = 0;
    }

    public void setWebContents(WebContents webContents) {
        assert mNativeContentViewRenderView != 0;
        mWebContents = webContents;

        if (webContents != null && getWidth() != 0 && getHeight() != 0) {
            updateWebContentsSize();
            maybeUpdatePhysicalBackingSize(mPhysicalWidth, mPhysicalHeight);
        }
        ContentViewRenderViewJni.get().setCurrentWebContents(
                mNativeContentViewRenderView, webContents);
    }

    public ResourceManager getResourceManager() {
        return ContentViewRenderViewJni.get().getResourceManager(mNativeContentViewRenderView);
    }

    public boolean hasSurface() {
        return mCompositorHasSurface;
    }

    @CalledByNative
    private boolean didSwapFrame() {
        assert mCurrent != null;
        return mCurrent.didSwapFrame();
    }

    @CalledByNative
    private void didSwapBuffers(boolean sizeMatches) {
        assert mCurrent != null;
        if (!sizeMatches) return;
        mCurrent.runSurfaceRedrawNeededCallbacks();
    }

    private void evictCachedSurface() {
        if (mNativeContentViewRenderView == 0) return;
        ContentViewRenderViewJni.get().evictCachedSurface(mNativeContentViewRenderView);
    }

    public long getNativeHandle() {
        return mNativeContentViewRenderView;
    }

    private void updateBackgroundColor() {
        int uiMode = getContext().getResources().getConfiguration().uiMode;
        boolean darkThemeEnabled =
                (uiMode & Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES;
        setBackgroundColor(darkThemeEnabled ? Color.BLACK : Color.WHITE);
    }

    @CalledByNative
    private int getBackgroundColor() {
        return mBackgroundColor;
    }

    private boolean shouldAvoidSurfaceResizeForSoftKeyboard() {
        // TextureView is more common with embedding use cases that should lead to resize.
        boolean usingSurfaceView = mCurrent != null && mCurrent.getMode() == MODE_SURFACE_VIEW;
        if (!usingSurfaceView) return false;

        boolean isFullWidth = isAttachedToWindow() && getWidth() == getRootView().getWidth();
        if (!isFullWidth) return false;

        InputMethodManager inputMethodManager =
                (InputMethodManager) getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        return inputMethodManager.isActive();
    }

    private void updateConfigChangeTimeStamp() {
        mConfigurationChangedTimestamp = SystemClock.uptimeMillis();
    }

    private void maybeUpdatePhysicalBackingSize(int width, int height) {
        if (mWebContents == null) return;
        boolean forConfigChange =
                SystemClock.uptimeMillis() - mConfigurationChangedTimestamp < CONFIG_TIMEOUT_MS;
        ContentViewRenderViewJni.get().onPhysicalBackingSizeChanged(
                mNativeContentViewRenderView, mWebContents, width, height, forConfigChange);
    }

    @NativeMethods
    interface Natives {
        long init(ContentViewRenderView caller, WindowAndroid rootWindow);
        void destroy(long nativeContentViewRenderView);
        void setCurrentWebContents(long nativeContentViewRenderView, WebContents webContents);
        void onPhysicalBackingSizeChanged(long nativeContentViewRenderView, WebContents webContents,
                int width, int height, boolean forConfigChange);
        void surfaceCreated(long nativeContentViewRenderView);
        void surfaceDestroyed(long nativeContentViewRenderView, boolean cacheBackBuffer);
        void surfaceChanged(long nativeContentViewRenderView, boolean canBeUsedWithSurfaceControl,
                int format, int width, int height, Surface surface);
        void setNeedsRedraw(long nativeContentViewRenderView);
        void evictCachedSurface(long nativeContentViewRenderView);
        ResourceManager getResourceManager(long nativeContentViewRenderView);
        void updateBackgroundColor(long nativeContentViewRenderView);
    }
}
