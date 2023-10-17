// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources.dynamics;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.HardwareRenderer;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.RenderNode;
import android.media.ImageReader;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.view.Surface;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter.CaptureMechanism;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.nio.ByteBuffer;

/**
 * Uses a {@link RenderNode} to perform bitmap capture of a java View. This should typically walk
 * the View hierarchy synchronously, populating a list of instructions. Then, on a separate thread,
 * the instructions are executed to paint colors onto a {@link Bitmap}. Uses functionality that
 * requires Android Q+.
 */
@RequiresApi(Build.VERSION_CODES.Q)
public class HardwareDraw implements CaptureMechanism {
    // AcceleratedImageReader starts in NEW, and on the first request for an Bitmap we move into
    // INITIALIZING, until we complete the first Bitmap when we move to UPDATED. We then on
    // future requests return the current Bitmap to prevent us blocking but send a new request
    // and move into RUNNING which prevents future requests from being queued. Once the request
    // is finished we move back to UPDATED and repeat.
    @IntDef({ImageReaderStatus.NEW, ImageReaderStatus.INITIALIZING, ImageReaderStatus.UPDATED,
            ImageReaderStatus.RUNNING})
    @Retention(RetentionPolicy.SOURCE)
    private @interface ImageReaderStatus {
        int NEW = 0;
        int INITIALIZING = 1;
        int UPDATED = 2;
        int RUNNING = 3;
    }

    private static class RequestState {
        // Track the last BitmapRequestId so we only return one image per request (in case of
        // animations during that draw).
        public final int requestId;

        public final View view;
        public final float scale;
        public final Callback<Bitmap> onBitmapCapture;

        private RequestState(
                int requestId, View view, float scale, Callback<Bitmap> onBitmapCapture) {
            this.requestId = requestId;
            this.view = view;
            this.scale = scale;
            this.onBitmapCapture = onBitmapCapture;
        }
        public static RequestState next(
                View view, float scale, Callback<Bitmap> onBitmapCapture, RequestState previous) {
            int nextId = previous == null ? 1 : previous.requestId + 1;
            return new RequestState(nextId, view, scale, onBitmapCapture);
        }
    }

    // RenderNode was added in API level 29 (Android 10). So restrict AcceleratedImageReader as
    // well.
    @RequiresApi(Build.VERSION_CODES.Q)
    private class AcceleratedImageReader implements ImageReader.OnImageAvailableListener {
        // Track the last BitmapRequestId so we only return one image per request (in case of
        // animations during that draw).
        private int mLastBitmapRequestId;
        private ImageReader mReaderDelegate;
        private @ImageReaderStatus int mImageReaderStatus;
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

        private AcceleratedImageReader(int width, int height) {
            mImageReaderStatus = ImageReaderStatus.NEW;
            mTaskRunner = PostTask.createSequencedTaskRunner(TaskTraits.USER_VISIBLE);
            init(width, height);
        }

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

        private void onLayoutChange(int width, int height) {
            // We want to prevent destroying mReaderDelegate if its going to be used so we use our
            // sequenced TaskRunner which will run after we finish with it.
            mTaskRunner.postTask(() -> { init(width, height); });
        }

        private @ImageReaderStatus int currentStatus() {
            return mImageReaderStatus;
        }

        private State currentState() {
            return mState;
        }

        // Should only be called directly after calling currentState() and seeing |requestNeeded|
        // being true. By requiring this we can avoid taking a lock to check the state before
        // posting the renderNode.
        private void requestDraw(
                RenderNode renderNode, View view, float scale, Callback<Bitmap> onBitmapCapture) {
            mThreadChecker.assertOnValidThread();
            switch (mImageReaderStatus) {
                case ImageReaderStatus.NEW:
                    mImageReaderStatus = ImageReaderStatus.INITIALIZING;
                    break;
                case ImageReaderStatus.UPDATED:
                    mImageReaderStatus = ImageReaderStatus.RUNNING;
                    break;
                case ImageReaderStatus.INITIALIZING:
                case ImageReaderStatus.RUNNING:
                    return;
            }

            assert renderNode != null;
            mTaskRunner.postTask(() -> {
                HardwareRenderer mRenderer = new HardwareRenderer();
                try (TraceEvent e = TraceEvent.scoped("AcceleratedImageReader::requestDraw")) {
                    mCurrentRequestState =
                            RequestState.next(view, scale, onBitmapCapture, mCurrentRequestState);
                    Surface s = mReaderDelegate.getSurface();
                    mRenderer.setContentRoot(renderNode);
                    mRenderer.setSurface(s);
                    mRenderer.createRenderRequest().syncAndDraw();
                    mRenderer.destroy();
                }
            });
        }

        // This method is run on the sHandler.
        @Override
        public void onImageAvailable(ImageReader reader) {
            try (TraceEvent e = TraceEvent.scoped("AcceleratedImageReader::onImageAvailable")) {
                // acquireLatestImage will discard any images in the queue up to the most recent
                // one.
                android.media.Image image = reader.acquireLatestImage();
                if (image == null) {
                    return;
                }

                final RequestState requestState = mCurrentRequestState;
                if (requestState.requestId == mLastBitmapRequestId) {
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
                mLastBitmapRequestId = requestState.requestId;

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
                        Bitmap snapshot =
                                CaptureUtils.createBitmap(width + rowPaddingInPixels, height);
                        snapshot.copyPixelsFromBuffer(buffer);
                        image.close();

                        requestState.view.getHandler().post(() -> {
                            final State currentState =
                                    new State(width, height, rowPaddingInPixels, snapshot);
                            mState = currentState;
                            int scaledWidth =
                                    (int) (requestState.view.getWidth() * requestState.scale);
                            int scaledHeight =
                                    (int) (requestState.view.getHeight() * requestState.scale);
                            if (mReader.validateCurrentBitmap(
                                        currentState, scaledWidth, scaledHeight)
                                    && currentState.mHardwareBitmap != null) {
                                requestState.onBitmapCapture.onResult(currentState.mHardwareBitmap);
                            }
                        });
                    }
                });
            }
        }

        private boolean validateCurrentBitmap(State state, int width, int height) {
            mThreadChecker.assertOnValidThread();
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

    // When using Hardware Acceleration the conversion from canvas to Bitmap occurs on a different
    // thread.
    private static Handler sHandler;

    private final ThreadUtils.ThreadChecker mThreadChecker = new ThreadUtils.ThreadChecker();

    @Nullable
    private AcceleratedImageReader mReader;
    private boolean mDebugViewAttachedToWindowListenerAdded;
    // Set each time we enqueue a Hardware drawn Bitmap.
    private RequestState mCurrentRequestState;

    /**
     * Each instance should be called by external clients only on the thread it is created. The
     * first instance created will also create a thread to do the actual rendering on.
     */
    public HardwareDraw() {
        mThreadChecker.assertOnValidThread();
        if (sHandler == null) {
            HandlerThread thread = new HandlerThread("HardwareDrawThread");
            thread.start();
            sHandler = new Handler(thread.getLooper());
        }
        // We couldn't set up |mReader| even if we had a View, because the view might not have had
        // its first layout yet and image reader needs to know the width and the height.
    }

    private boolean captureHardware(Canvas canvas, View view, Rect dirtyRect, float scale,
            boolean drawWhileDetached, CaptureObserver observer) {
        if (CaptureUtils.captureCommon(canvas, view, dirtyRect, scale,
                    /*drawWhileDetached*/ drawWhileDetached, observer)) {
            return true;
        }
        // TODO(https://crbug/1318009): Remove this code or promote it to default once we determine
        TraceEvent.instant("HardwareDraw::DrawAttemptedWhileDetached");
        if (!mDebugViewAttachedToWindowListenerAdded) {
            mDebugViewAttachedToWindowListenerAdded = true;
            view.addOnAttachStateChangeListener(new View.OnAttachStateChangeListener() {
                @Override
                public void onViewAttachedToWindow(View view) {
                    TraceEvent.instant("HardwareDraw::ViewAttachedToWindow");
                    view.removeOnAttachStateChangeListener(this);
                    mDebugViewAttachedToWindowListenerAdded = false;
                }

                @Override
                public void onViewDetachedFromWindow(View view) {
                    TraceEvent.instant("HardwareDraw::ViewDetachedFromWindow");
                }
            });
        }
        return false;
    }

    /**
     * This uses a RecordingNode to store all the required draw instructions without doing them
     * upfront. And then on a threadpool task we grab a hardware canvas (required to use a
     * RenderNode) and draw it using the hardware accelerated canvas.
     * @return If draw instructions were recorded and the dirty rect can be reset.
     */
    private boolean captureWithHardwareDraw(View view, Rect dirtyRect, float scale,
            CaptureObserver observer, Callback<Bitmap> onBitmapCapture) {
        try (TraceEvent e = TraceEvent.scoped("HardwareDraw:captureWithHardwareDraw")) {
            if (view.getWidth() == 0 || view.getHeight() == 0) {
                // We haven't actually laid out this view yet no point in requesting a screenshot.
                // Keep this in sync with other restrictions in init().
                return false;
            }

            // Since state is replaced with a whole new object on a different thread if we grab a
            // reference (which is atomic in java) we can ensure the only thread that is going to
            // modify this state object is the UI thread. So grab it all up front.
            AcceleratedImageReader.State currentState = mReader.currentState();

            // If we didn't have a bitmap to return and there isn't an ongoing request already we
            // will start a bitmap copy which will be done Async on a different thread.
            RenderNode renderNode = null;
            if (currentState.mRequestNewDraw && !dirtyRect.isEmpty()) {
                // TODO(nuskos): There are potential optimizations here.
                //                   1) We could save the RenderNode if nothing has changed and all
                //                      we need is a new draw.
                //                   2) Instead of using mScale we could just do
                //                      renderNode.setScaleX & setScaleY, again reusing the
                //                      RenderNode because really it was just a redraw that was
                //                      needed.
                renderNode = new RenderNode("bitmapRenderNode");
                renderNode.setPosition(0, 0, view.getWidth(), view.getHeight());

                Canvas canvas = renderNode.beginRecording();
                boolean captureSuccess = captureHardware(
                        canvas, view, dirtyRect, scale, /*drawWhileDetached*/ false, observer);
                renderNode.endRecording();
                if (captureSuccess) {
                    onDrawInstructionsAvailable(
                            renderNode, currentState, view, scale, onBitmapCapture);
                }
                return captureSuccess;
            }
            return false;
        }
    }

    @SuppressWarnings("NewApi")
    private void onDrawInstructionsAvailable(RenderNode renderNode,
            AcceleratedImageReader.State currentState, View view, float scale,
            Callback<Bitmap> onBitmapCapture) {
        currentState.mRequestNewDraw = false;
        mReader.requestDraw(renderNode, view, scale, onBitmapCapture);
    }

    @Override
    public boolean shouldRemoveResourceOnNullBitmap() {
        return true;
    }

    @Override
    public void onViewSizeChange(View view, float scale) {
        int scaledWidth = (int) (view.getWidth() * scale);
        int scaledHeight = (int) (view.getHeight() * scale);
        if (mReader == null) {
            mReader = new AcceleratedImageReader(scaledWidth, scaledHeight);
        } else {
            mReader.onLayoutChange(scaledWidth, scaledHeight);
        }
    }

    @Override
    public void dropCachedBitmap() {
        if (mReader == null || mReader.mState == null) {
            return;
        }
        mReader.mState.mHardwareBitmap = null;
    }

    @Override
    public boolean startBitmapCapture(View view, Rect dirtyRect, float scale,
            CaptureObserver observer, Callback<Bitmap> onBitmapCapture) {
        try (TraceEvent e = TraceEvent.scoped("HardwareDraw:syncCaptureBitmap")) {
            return captureWithHardwareDraw(view, dirtyRect, scale, observer, onBitmapCapture);
        }
    }
}