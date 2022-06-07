// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources.dynamics;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.HardwareRenderer;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.RenderNode;
import android.media.ImageReader;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.SystemClock;
import android.view.Surface;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;

import androidx.annotation.RequiresApi;

import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.ui.resources.Resource;
import org.chromium.ui.resources.ResourceFactory;
import org.chromium.ui.resources.statics.NinePatchData;

import java.nio.ByteBuffer;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * An adapter that exposes a {@link View} as a {@link DynamicResource}. In order to properly use
 * this adapter {@link ViewResourceAdapter#invalidate(Rect)} must be called when parts of the
 * {@link View} are invalidated.  For {@link ViewGroup}s the easiest way to do this is to override
 * {@link ViewGroup#invalidateChildInParent(int[], Rect)}.
 */
public class ViewResourceAdapter extends DynamicResource implements OnLayoutChangeListener {
    private final View mView;
    private final Rect mDirtyRect = new Rect();

    private Bitmap mBitmap;
    private Rect mViewSize = new Rect();
    protected float mScale = 1;
    private long mLastGetBitmapTimestamp;
    private AcceleratedImageReader mReader;
    private boolean mUseHardwareBitmapDraw;
    private boolean mDebugViewAttachedToWindowListenerAdded;
    // Incremented each time we enqueue a Hardware drawn Bitmap. Only used if
    // |mUseHardwareBitmapDraw| is true.
    protected AtomicInteger mCurrentBitmapRequestId;

    // When using Hardware Acceleration the conversion from canvas to Bitmap occurs on a different
    // thread.
    protected static Handler sHandler;
    // AcceleratedImageReader starts in NEW, and on the first request for an Bitmap we move into
    // INITIALIZING, until we complete the first Bitmap when we move to UPDATED. We then on
    // future requests return the current Bitmap to prevent us blocking but send a new request
    // and move into RUNNING which prevents future requests from being queued. Once the request
    // is finished we move back to UPDATED and repeat.
    public enum ImageReaderStatus { NEW, INITIALIZING, UPDATED, RUNNING }
    private ThreadUtils.ThreadChecker mAdapterThreadChecker = new ThreadUtils.ThreadChecker();

    // RenderNode was added in API level 29 (Android 10). So restrict AcceleratedImageReader as
    // well.
    @RequiresApi(Build.VERSION_CODES.Q)
    private class AcceleratedImageReader implements ImageReader.OnImageAvailableListener {
        // Track the last BitmapRequestId so we only return one image per request (in case of
        // animations during that draw).
        private int mLastBitmapRequestId;
        private ImageReader mReaderDelegate;
        private ImageReaderStatus mImageReaderStatus;
        // To prevent having to take synchronized locks on the UI thread the hardware acceleration
        // code writes to mSnapshot and then atomically writes the reference into mHardwareBitmap.
        // This allows the UI thread to just grab the reference which will eventually update to the
        // new value.
        private State mState;
        private SequencedTaskRunner mTaskRunner;
        // Simple holder of the current State, this includes if a new bitmap is needed (i.e. no
        // ongoing requests), and the conditions the bitmap were taken with.
        private class State {
            public int mWidth;
            public int mHeight;
            public int mRowPaddingInPixels;
            public Bitmap mHardwareBitmap;
            public boolean mRequestNewDraw;

            State(int width, int height, int padding, Bitmap bitmap) {
                mWidth = width;
                mHeight = height;
                mRowPaddingInPixels = padding;
                mHardwareBitmap = bitmap;
                mRequestNewDraw = true;
            }
        }

        AcceleratedImageReader(int width, int height) {
            mImageReaderStatus = ImageReaderStatus.NEW;
            mTaskRunner = PostTask.createSequencedTaskRunner(TaskTraits.USER_VISIBLE);
            init(width, height);
        }

        // This needs PixelFormat.RGBA_8888, because the |mBitmap| uses Bitmap.Config.ARGB_888
        // this is supported by the android docs which states " This must be one of the
        // ImageFormat or PixelFormat constants.". It does state that not all formats are
        // supported, but this seems to work and has worked for quite awhile. This comment
        // exists because of a lint error that we suppress below.
        @SuppressWarnings("WrongConstant")
        private void init(int width, int height) {
            try (TraceEvent e = TraceEvent.scoped("AcceleratedImageReader::init")) {
                // Due to how ImageReader works, it is an error to attempt to acquire more than
                // |maxBitmapsToAcquire|. We need 1 acquire to convert to a bitmap, and 2 to acquire
                // and discard any images that are queued (by animations).
                final int maxBitmapsToAcquire = 3;
                if (mReaderDelegate != null) {
                    // Inform the GPU that we can clean up the ImageReader and associated GPU memory
                    // and contexts, prevents unneeded callbacks as well.
                    mReaderDelegate.close();
                }
                if (width == 0 || height == 0) {
                    // If we get less than 1 width or height, there is nothing to draw.
                    // Keep this in sync with other restrictions in captureWithHardwareDraw().
                    return;
                }
                mReaderDelegate = ImageReader.newInstance(
                        width, height, PixelFormat.RGBA_8888, maxBitmapsToAcquire);
                mReaderDelegate.setOnImageAvailableListener(this, sHandler);
                mState = new State(0, 0, 0, null);
            }
        }

        public void onLayoutChange(int width, int height) {
            // We want to prevent destroying mReaderDelegate if its going to be used so we use our
            // sequenced TaskRunner which will run after we finish with it.
            mTaskRunner.postTask(() -> { init(width, height); });
        }

        public ImageReaderStatus currentStatus() {
            return mImageReaderStatus;
        }

        public State currentState() {
            return mState;
        }

        // Should only be called directly after calling currentState() and seeing |requestNeeded|
        // being true. By requiring this we can avoid taking a lock to check the state before
        // posting the renderNode.
        public void requestDraw(RenderNode renderNode) {
            mAdapterThreadChecker.assertOnValidThread();
            switch (mImageReaderStatus) {
                case NEW:
                    mImageReaderStatus = ImageReaderStatus.INITIALIZING;
                    break;
                case UPDATED:
                    mImageReaderStatus = ImageReaderStatus.RUNNING;
                    break;
                case INITIALIZING:
                case RUNNING:
                    return;
            }

            assert renderNode != null;
            mTaskRunner.postTask(() -> {
                HardwareRenderer mRenderer = new HardwareRenderer();
                try (TraceEvent e = TraceEvent.scoped("AcceleratedImageReader::requestDraw")) {
                    mCurrentBitmapRequestId.incrementAndGet();
                    Surface s = mReaderDelegate.getSurface();
                    mRenderer.setContentRoot(renderNode);
                    mRenderer.setSurface(s);
                    mRenderer.createRenderRequest().syncAndDraw();
                    mRenderer.destroy();
                }
            });
        }

        @Override
        public void onImageAvailable(ImageReader reader) {
            try (TraceEvent e = TraceEvent.scoped("AcceleratedImageReader::onImageAvailable")) {
                // acquireLatestImage will discard any images in the queue up to the most recent
                // one.
                android.media.Image image = reader.acquireLatestImage();
                if (image == null) {
                    return;
                }

                int request = mCurrentBitmapRequestId.get();
                if (request == mLastBitmapRequestId) {
                    // If there was an animation when we requested a draw, we will receive each
                    // frame of the animation. For now we just take the first one (though the last
                    // would be better there is no good way to know when its the last of the frame).
                    //
                    // TODO(nuskos): We should either a) not grab a bitmap with an animation at all
                    //               (likely the image we had before the animation is actually
                    //               correct and we're just doing extra work). Or we should figure
                    //               out how to get the last one.
                    image.close();
                    return;
                }
                mLastBitmapRequestId = request;

                android.media.Image.Plane[] planes = image.getPlanes();
                assert planes.length != 0;
                ByteBuffer buffer = planes[0].getBuffer();
                assert buffer != null;

                mTaskRunner.postTask(() -> {
                    try (TraceEvent e2 = TraceEvent.scoped(
                                 "AcceleratedImageReader::onImageAvailable::postTask")) {
                        final int width = image.getWidth();
                        final int height = image.getHeight();
                        final int pixelStride = planes[0].getPixelStride();
                        final int rowPaddingInBytes =
                                planes[0].getRowStride() - pixelStride * width;
                        final int rowPaddingInPixels = rowPaddingInBytes / pixelStride;
                        // Set |mHardwareBitmap| to use the correct padding so it will copy from the
                        // buffer.
                        Bitmap snapshot = createBitmap(width + rowPaddingInPixels, height);
                        snapshot.copyPixelsFromBuffer(buffer);
                        image.close();
                        // Update the bitmap the UI reads.
                        mState = new State(width, height, rowPaddingInPixels, snapshot);
                    }
                });
            }
        }

        public boolean validateCurrentBitmap(State state, int width, int height) {
            mAdapterThreadChecker.assertOnValidThread();
            if (state.mHardwareBitmap == null || state.mWidth != width || state.mHeight != height) {
                // The current image isn't for our layout.
                return false;
            }
            // The bitmap that we have computed will now be used and we can stop returning "true"
            // for isDirty() until the next bitmap is requested.
            mImageReaderStatus = ImageReaderStatus.UPDATED;

            if (state.mRowPaddingInPixels == 0) {
                return true;
            }
            // Crop out the row padding that the android.media.Image contained. Since this state is
            // a reference this update will be there the next time and we won't recreate this bitmap
            // we'll just return true and use it as stored in the state object.
            Bitmap src = state.mHardwareBitmap;
            state.mHardwareBitmap = Bitmap.createBitmap(src, 0, 0, width, height);
            state.mHardwareBitmap.setHasAlpha(true);
            state.mRowPaddingInPixels = 0;
            src.recycle();
            return true;
        }
    }

    /**
     * Builds a {@link ViewResourceAdapter} instance around {@code view}.
     * @param view The {@link View} to expose as a {@link Resource}.
     *
     * @param useHardwareBitmapDraw controls if we should software draw bitmaps or use a
     * RenderNode and hardware acceleration.
     */
    public ViewResourceAdapter(View view, boolean useHardwareBitmapDraw) {
        mView = view;
        mDebugViewAttachedToWindowListenerAdded = false;
        mView.addOnLayoutChangeListener(this);
        mDirtyRect.set(0, 0, mView.getWidth(), mView.getHeight());
        // Enforce hardware accelerated drawing on android Q+ where it's supported.
        mUseHardwareBitmapDraw =
                useHardwareBitmapDraw && Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q;
        if (mUseHardwareBitmapDraw) {
            if (sHandler == null) {
                HandlerThread thread = new HandlerThread("ViewResourceAdapterThread");
                thread.start();
                sHandler = new Handler(thread.getLooper());
            }
            mCurrentBitmapRequestId = new AtomicInteger(0);
            // We can't set up |mReader| here because |mView| might not have had its first layout
            // yet and image reader needs to know the width and the height.
        }
    }

    /**
     * Builds a {@link ViewResourceAdapter} instance around {@code view}.
     * @param view The {@link View} to expose as a {@link Resource}.
     */
    public ViewResourceAdapter(View view) {
        this(view, false);
    }

    private Bitmap createBitmap(int width, int height) {
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        bitmap.setHasAlpha(true);
        return bitmap;
    }

    // This uses a RecordingNode to store all the required draw instructions without doing
    // them upfront. And then on a threadpool task we grab a hardware canvas (required to use a
    // RenderNode) and draw it using the hardware accelerated canvas.
    @RequiresApi(Build.VERSION_CODES.Q)
    private boolean captureWithHardwareDraw() {
        try (TraceEvent e = TraceEvent.scoped("ViewResourceAdapter:captureWithHardwareDraw")) {
            if (mView.getWidth() == 0 || mView.getHeight() == 0) {
                // We haven't actually laid out this view yet no point in requesting a screenshot.
                // Keep this in sync with other restrictions in init().
                return false;
            }

            // Since state is replaced with a whole new object on a different thread if we grab a
            // reference (which is atomic in java) we can ensure the only thread that is going to
            // modify this state object is the UI thread. So grab it all up front.
            AcceleratedImageReader.State currentState = mReader.currentState();

            // If we have a new Bitmap update our |mBitmap| to the newest version. Since we update
            // one check late we might serve a slightly stale result but not for long. If the bitmap
            // is not there we'll end up just showing a blank toolbar without any icons or text.
            // However this is preferred over blocking the main thread waiting for the image
            // potentially during user input.
            int scaledWidth = (int) (mView.getWidth() * mScale);
            int scaledHeight = (int) (mView.getHeight() * mScale);
            if (mReader.validateCurrentBitmap(currentState, scaledWidth, scaledHeight)
                    && currentState.mHardwareBitmap != null) {
                mBitmap = currentState.mHardwareBitmap;
            }

            // If we didn't have a bitmap to return and there isn't an ongoing request already we
            // will start a bitmap copy which will be done Async on a different thread.
            RenderNode renderNode = null;
            if (currentState.mRequestNewDraw && !mDirtyRect.isEmpty()) {
                // TODO(nuskos): There are potential optimizations here.
                //                   1) We could save the RenderNode if nothing has changed and all
                //                      we need is a new draw.
                //                   2) Instead of using mScale we could just do
                //                      renderNode.setScaleX & setScaleY, again reusing the
                //                      RenderNode because really it was just a redraw that was
                //                      needed.
                renderNode = new RenderNode("bitmapRenderNode");
                renderNode.setPosition(0, 0, mView.getWidth(), mView.getHeight());

                Canvas canvas = renderNode.beginRecording();
                boolean captureSuccess = captureHardware(canvas, false);
                renderNode.endRecording();
                if (captureSuccess) {
                    onDrawInstructionsAvailable(renderNode, currentState);
                }
                return captureSuccess;
            }
            return true;
        }
    }

    @SuppressWarnings("NewApi")
    private void onDrawInstructionsAvailable(
            RenderNode renderNode, AcceleratedImageReader.State currentState) {
        currentState.mRequestNewDraw = false;
        mReader.requestDraw(renderNode);
    }

    private void captureWithSoftwareDraw() {
        try (TraceEvent e = TraceEvent.scoped("ViewResourceAdapter:captureWithSoftwareDraw")) {
            Canvas canvas = new Canvas(mBitmap);
            captureCommon(canvas, true);
        }
    }

    /**
     * If this resource is dirty ({@link #isDirty()} returned {@code true}), it will recapture a
     * {@link Bitmap} of the {@link View}.
     * @see DynamicResource#getBitmap()
     * @return A {@link Bitmap} representing the {@link View}.
     */
    @Override
    @SuppressWarnings("NewApi")
    public Bitmap getBitmap() {
        mAdapterThreadChecker.assertOnValidThread();
        TraceEvent.begin("ViewResourceAdapter:getBitmap");
        super.getBitmap();
        boolean bitmapReady = false;
        if (mLastGetBitmapTimestamp > 0) {
            RecordHistogram.recordLongTimesHistogram("ViewResourceAdapter.GetBitmapInterval",
                    SystemClock.elapsedRealtime() - mLastGetBitmapTimestamp);
        }

        if (mUseHardwareBitmapDraw) {
            bitmapReady = captureWithHardwareDraw();
        } else if (validateBitmap()) {
            captureWithSoftwareDraw();
            bitmapReady = true;
        } else {
            assert mBitmap.getWidth() == 1 && mBitmap.getHeight() == 1;
            mBitmap.setPixel(0, 0, Color.TRANSPARENT);
        }

        if (bitmapReady) {
            mDirtyRect.setEmpty();
            mLastGetBitmapTimestamp = SystemClock.elapsedRealtime();
        }

        TraceEvent.end("ViewResourceAdapter:getBitmap");
        return mBitmap;
    }

    @Override
    public boolean shouldRemoveResourceOnNullBitmap() {
        return mUseHardwareBitmapDraw;
    }

    @Override
    public Rect getBitmapSize() {
        return mViewSize;
    }

    /**
     * Set the downsampling scale. The rendered size is not affected.
     * @param scale The scale to use. <1 means the Bitmap is smaller than the View.
     */
    public void setDownsamplingScale(float scale) {
        assert scale <= 1;
        if (mScale != scale) {
            invalidate(null);
        }
        mScale = scale;
    }

    /**
     * Override this method to create the native resource type for the generated bitmap.
     */
    @Override
    public long createNativeResource() {
        return ResourceFactory.createBitmapResource(null);
    }

    @Override
    public final NinePatchData getNinePatchData() {
        return null;
    }

    @Override
    public boolean isDirty() {
        // The bitmap is dirty if some part of it has changed, or in hardware mode we're waiting for
        // the results of a previous request (null mBitmap or RUNNING).
        return !mDirtyRect.isEmpty()
                || (mUseHardwareBitmapDraw
                        && (mBitmap == null
                                || mReader.currentStatus() == ImageReaderStatus.RUNNING));
    }

    @Override
    public void onLayoutChange(View v, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        final int width = right - left;
        final int height = bottom - top;
        final int oldWidth = oldRight - oldLeft;
        final int oldHeight = oldBottom - oldTop;

        if (width != oldWidth || height != oldHeight) {
            mViewSize.set(0, 0, width, height);
            mDirtyRect.set(0, 0, width, height);
            if (mUseHardwareBitmapDraw) {
                // Wait for a new image before returning anything.
                if (mBitmap != null) {
                    mBitmap.recycle();
                    mBitmap = null;
                }
                int scaledWidth = (int) (mView.getWidth() * mScale);
                int scaledHeight = (int) (mView.getHeight() * mScale);
                if (mReader == null) {
                    mReader = new AcceleratedImageReader(scaledWidth, scaledHeight);
                } else {
                    mReader.onLayoutChange(scaledWidth, scaledHeight);
                }
            }
        }
    }

    /**
     * Invalidates a particular region of the {@link View} that needs to be repainted.
     * @param dirtyRect The region to invalidate, or {@code null} if the entire {@code Bitmap}
     *                  should be redrawn.
     */
    public void invalidate(Rect dirtyRect) {
        if (dirtyRect == null) {
            mDirtyRect.set(0, 0, mView.getWidth(), mView.getHeight());
        } else {
            mDirtyRect.union(dirtyRect);
        }
    }

    /**
     * Drops the cached bitmap to free up memory.
     */
    public void dropCachedBitmap() {
        mBitmap = null;
    }

    /**
     * @return Dirty rect that will be drawn on capture.
     */
    protected Rect getDirtyRect() {
        return mDirtyRect;
    }

    /**
     * Called before {@link #capture(Canvas)} is called.
     * @param canvas    The {@link Canvas} that will be drawn to.
     * @param dirtyRect The dirty {@link Rect} or {@code null} if the entire area is being redrawn.
     */
    protected void onCaptureStart(Canvas canvas, Rect dirtyRect) {
    }

    /**
     * Called to draw the {@link View}'s contents into the passed in {@link Canvas}.
     * @param canvas The {@link Canvas} that will be drawn to.
     */
    protected void capture(Canvas canvas) {
        canvas.save();
        canvas.scale(mScale, mScale);
        mView.draw(canvas);
        canvas.restore();
    }

    /**
     * Called to draw the {@link View}'s contents into the passed in {@link Canvas}.
     * @param canvas The {@link Canvas} that will be drawn to.
     * @param drawWhileDetached drawing while detached causes crashes for both software and
     * hardware renderer, since enabling hardware renderer caused a regression in number of
     * crashes, this boolean will only be true for software renderer, and will be removed
     * later on if the issue was fixed for the hardware renderer and logic for avoiding the
     * draw would be the same for both hardware and software renderer.
     * Software or hardware draw will both need to follow this pattern.
     * @return true if the draw is successful, false if we couldn't draw because the view is
     * detached.
     */
    protected boolean captureCommon(Canvas canvas, boolean drawWhileDetached) {
        boolean willDraw = drawWhileDetached || mView.isAttachedToWindow();
        if (!willDraw) {
            return false;
        }
        onCaptureStart(canvas, mDirtyRect.isEmpty() ? null : mDirtyRect);
        if (!mDirtyRect.isEmpty()) {
            canvas.clipRect(mDirtyRect);
        }
        capture(canvas);
        onCaptureEnd();
        return true;
    }

    protected boolean captureHardware(Canvas canvas, boolean drawWhileDetached) {
        if (captureCommon(canvas, drawWhileDetached)) {
            return true;
        }
        // TODO(crbug/1318009): remove this code or promote it to default once we determine if this
        // is the proper fix.
        TraceEvent.instant("ViewResourceAdapter::DrawAttemptedWhileDetached");
        if (!mDebugViewAttachedToWindowListenerAdded) {
            mDebugViewAttachedToWindowListenerAdded = true;
            mView.addOnAttachStateChangeListener(new View.OnAttachStateChangeListener() {
                @Override
                public void onViewAttachedToWindow(View view) {
                    TraceEvent.instant("ViewResourceAdapter::ViewAttachedToWindow");
                    view.removeOnAttachStateChangeListener(this);
                    mDebugViewAttachedToWindowListenerAdded = false;
                }

                @Override
                public void onViewDetachedFromWindow(View view) {
                    TraceEvent.instant("ViewResourceAdapter::ViewDetachedFromWindow");
                }
            });
        }
        return false;
    }

    /**
     * Called after {@link #capture(Canvas)}.
     */
    protected void onCaptureEnd() {
    }

    /**
     * @return Whether |mBitmap| is corresponding to |mView| or not.
     */
    private boolean validateBitmap() {
        int viewWidth = (int) (mView.getWidth() * mScale);
        int viewHeight = (int) (mView.getHeight() * mScale);
        boolean isEmpty = viewWidth == 0 || viewHeight == 0;
        if (isEmpty) {
            viewWidth = 1;
            viewHeight = 1;
        }
        if (mBitmap != null
                && (mBitmap.getWidth() != viewWidth || mBitmap.getHeight() != viewHeight)) {
            mBitmap.recycle();
            mBitmap = null;
        }

        if (mBitmap == null) {
            mBitmap = createBitmap(viewWidth, viewHeight);
            mViewSize.set(0, 0, mView.getWidth(), mView.getHeight());
            mDirtyRect.set(mViewSize);
        }

        return !isEmpty;
    }
}
