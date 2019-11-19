// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.annotation.TargetApi;
import android.content.ClipData;
import android.content.ClipDescription;
import android.os.Build;
import android.view.DragEvent;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Class used to forward view, input events down to native.
 */
@JNINamespace("ui")
public class EventForwarder {
    private final boolean mIsDragDropEnabled;

    private long mNativeEventForwarder;

    // Offsets for the events that passes through.
    private float mCurrentTouchOffsetX;
    private float mCurrentTouchOffsetY;

    private int mLastMouseButtonState;

    @CalledByNative
    private static EventForwarder create(long nativeEventForwarder, boolean isDragDropEnabled) {
        return new EventForwarder(nativeEventForwarder, isDragDropEnabled);
    }

    private EventForwarder(long nativeEventForwarder, boolean isDragDropEnabled) {
        mNativeEventForwarder = nativeEventForwarder;
        mIsDragDropEnabled = isDragDropEnabled;
    }

    @CalledByNative
    private void destroy() {
        mNativeEventForwarder = 0;
    }

    // Returns the scaling being applied to the event's source. Typically only used for VR when
    // drawing Android UI to a texture.
    private float getEventSourceScaling() {
        return EventForwarderJni.get()
                .getJavaWindowAndroid(mNativeEventForwarder, EventForwarder.this)
                .getDisplay()
                .getAndroidUIScaling();
    }

    private boolean hasTouchEventOffset() {
        return mCurrentTouchOffsetX != 0.0f || mCurrentTouchOffsetY != 0.0f;
    }

    /**
     * @see View#onTouchEvent(MotionEvent)
     */
    public boolean onTouchEvent(MotionEvent event) {
        // TODO(mustaq): Should we include MotionEvent.TOOL_TYPE_STYLUS here?
        // crbug.com/592082
        if (event.getToolType(0) == MotionEvent.TOOL_TYPE_MOUSE) {
            // Skip firing mouse events in the follwoing cases:
            // - In Android L and below, where mouse button info is incomplete.
            // - A move w/o a button press, which represents a trackpad scroll. Real mouse moves w/o
            //   buttons goes to onHoverEvent.
            final int apiVersion = Build.VERSION.SDK_INT;
            final boolean isTouchpadScroll = event.getButtonState() == 0
                    && (event.getActionMasked() == MotionEvent.ACTION_DOWN
                               || event.getActionMasked() == MotionEvent.ACTION_MOVE
                               || event.getActionMasked() == MotionEvent.ACTION_UP);

            if (apiVersion >= android.os.Build.VERSION_CODES.M && !isTouchpadScroll) {
                return onMouseEvent(event);
            }
        }

        final boolean isTouchHandleEvent = false;
        return sendTouchEvent(event, isTouchHandleEvent);
    }

    /**
     * Called by PopupWindow-based touch handles.
     * @param event the MotionEvent targeting the handle.
     */
    public boolean onTouchHandleEvent(MotionEvent event) {
        final boolean isTouchHandleEvent = true;
        return sendTouchEvent(event, isTouchHandleEvent);
    }

    private boolean sendTouchEvent(MotionEvent event, boolean isTouchHandleEvent) {
        assert mNativeEventForwarder != 0;

        TraceEvent.begin("sendTouchEvent");
        try {
            // Android may batch multiple events together for efficiency. We
            // want to use the oldest event time as hardware time stamp.
            final long oldestEventTime = event.getHistorySize() > 0
                    ? event.getHistoricalEventTime(0)
                    : event.getEventTime();

            int eventAction = event.getActionMasked();

            eventAction = SPenSupport.convertSPenEventAction(eventAction);

            if (!isValidTouchEventActionForNative(eventAction)) return false;

            // A zero offset is quite common, in which case the unnecessary copy should be avoided.
            boolean didOffsetEvent = false;
            if (hasTouchEventOffset()) {
                event = createOffsetMotionEventIfNeeded(event);
                didOffsetEvent = true;
            }

            final int pointerCount = event.getPointerCount();

            float[] touchMajor = {
                    event.getTouchMajor(), pointerCount > 1 ? event.getTouchMajor(1) : 0};
            float[] touchMinor = {
                    event.getTouchMinor(), pointerCount > 1 ? event.getTouchMinor(1) : 0};

            for (int i = 0; i < 2; i++) {
                if (touchMajor[i] < touchMinor[i]) {
                    float tmp = touchMajor[i];
                    touchMajor[i] = touchMinor[i];
                    touchMinor[i] = tmp;
                }
            }

            float secondPointerX = pointerCount > 1 ? event.getX(1) : 0;
            float secondPointerY = pointerCount > 1 ? event.getY(1) : 0;

            float scale = getEventSourceScaling();

            final boolean consumed = EventForwarderJni.get().onTouchEvent(mNativeEventForwarder,
                    EventForwarder.this, event, oldestEventTime, eventAction, pointerCount,
                    event.getHistorySize(), event.getActionIndex(), event.getX() / scale,
                    event.getY() / scale, secondPointerX / scale, secondPointerY / scale,
                    event.getPointerId(0), pointerCount > 1 ? event.getPointerId(1) : -1,
                    touchMajor[0] / scale, touchMajor[1] / scale, touchMinor[0] / scale,
                    touchMinor[1] / scale, event.getOrientation(),
                    pointerCount > 1 ? event.getOrientation(1) : 0,
                    event.getAxisValue(MotionEvent.AXIS_TILT),
                    pointerCount > 1 ? event.getAxisValue(MotionEvent.AXIS_TILT, 1) : 0,
                    event.getRawX() / scale, event.getRawY() / scale, event.getToolType(0),
                    pointerCount > 1 ? event.getToolType(1) : MotionEvent.TOOL_TYPE_UNKNOWN,
                    event.getButtonState(), event.getMetaState(), isTouchHandleEvent);

            if (didOffsetEvent) event.recycle();
            return consumed;
        } finally {
            TraceEvent.end("sendTouchEvent");
        }
    }

    /**
     * Sets the current amount to offset incoming touch events by (including MotionEvent and
     * DragEvent). This is used to handle content moving and not lining up properly with the
     * android input system.
     * @param dx The X offset in pixels to shift touch events.
     * @param dy The Y offset in pixels to shift touch events.
     */
    public void setCurrentTouchEventOffsets(float dx, float dy) {
        mCurrentTouchOffsetX = dx;
        mCurrentTouchOffsetY = dy;
    }

    /**
     * Creates a new motion event differed from the given event by current touch offset
     * if the offset is not zero.
     * @param src Source motion event.
     * @return A new motion event if we have non-zero touch offset. Otherwise return the same event.
     */
    public MotionEvent createOffsetMotionEventIfNeeded(MotionEvent src) {
        if (!hasTouchEventOffset()) return src;
        MotionEvent dst = MotionEvent.obtain(src);
        dst.offsetLocation(mCurrentTouchOffsetX, mCurrentTouchOffsetY);
        return dst;
    }

    private static boolean isValidTouchEventActionForNative(int eventAction) {
        // Only these actions have any effect on gesture detection.  Other
        // actions have no corresponding WebTouchEvent type and may confuse the
        // touch pipline, so we ignore them entirely.
        return eventAction == MotionEvent.ACTION_DOWN || eventAction == MotionEvent.ACTION_UP
                || eventAction == MotionEvent.ACTION_CANCEL
                || eventAction == MotionEvent.ACTION_MOVE
                || eventAction == MotionEvent.ACTION_POINTER_DOWN
                || eventAction == MotionEvent.ACTION_POINTER_UP;
    }

    /**
     * @see View#onHoverEvent(MotionEvent)
     */
    public boolean onHoverEvent(MotionEvent event) {
        TraceEvent.begin("onHoverEvent");
        boolean didOffsetEvent = false;
        try {
            if (hasTouchEventOffset()) {
                event = createOffsetMotionEventIfNeeded(event);
                didOffsetEvent = true;
            }
            // Work around Samsung Galaxy Tab 2 not sending ACTION_BUTTON_RELEASE on left-click:
            // http://crbug.com/714230.  On ACTION_HOVER, no button can be pressed, so send a
            // synthetic ACTION_BUTTON_RELEASE if it was missing.  Note that all
            // ACTION_BUTTON_RELEASE are always fired before any hover events on a correctly
            // behaving device, so mLastMouseButtonState is only nonzero on a buggy one.
            int eventAction = event.getActionMasked();
            if (eventAction == MotionEvent.ACTION_HOVER_ENTER) {
                if (mLastMouseButtonState == MotionEvent.BUTTON_PRIMARY) {
                    float scale = getEventSourceScaling();
                    EventForwarderJni.get().onMouseEvent(mNativeEventForwarder, EventForwarder.this,
                            event.getEventTime(), MotionEvent.ACTION_BUTTON_RELEASE,
                            event.getX() / scale, event.getY() / scale, event.getPointerId(0),
                            event.getPressure(0), event.getOrientation(0),
                            event.getAxisValue(MotionEvent.AXIS_TILT, 0),
                            MotionEvent.BUTTON_PRIMARY, event.getButtonState(),
                            event.getMetaState(), event.getToolType(0));
                }
                mLastMouseButtonState = 0;
            }
            return sendNativeMouseEvent(event);
        } finally {
            if (didOffsetEvent) event.recycle();
            TraceEvent.end("onHoverEvent");
        }
    }

    /**
     * @see View#onMouseEvent(MotionEvent)
     */
    public boolean onMouseEvent(MotionEvent event) {
        TraceEvent.begin("sendMouseEvent");
        boolean didOffsetEvent = false;
        try {
            if (hasTouchEventOffset()) {
                event = createOffsetMotionEventIfNeeded(event);
                didOffsetEvent = true;
            }
            updateMouseEventState(event);
            return sendNativeMouseEvent(event);
        } finally {
            if (didOffsetEvent) event.recycle();
            TraceEvent.end("sendMouseEvent");
        }
    }

    /**
     * Sends mouse event to native. Hover event is also converted to mouse event,
     * only differentiated by an internal flag.
     */
    private boolean sendNativeMouseEvent(MotionEvent event) {
        assert mNativeEventForwarder != 0;

        int eventAction = event.getActionMasked();

        // Ignore ACTION_HOVER_ENTER & ACTION_HOVER_EXIT because every mouse-down on Android
        // follows a hover-exit and is followed by a hover-enter.  https://crbug.com/715114
        // filed on distinguishing actual hover enter/exit from these bogus ones.
        if (eventAction == MotionEvent.ACTION_HOVER_ENTER
                || eventAction == MotionEvent.ACTION_HOVER_EXIT) {
            return false;
        }

        // For mousedown and mouseup events, we use ACTION_BUTTON_PRESS
        // and ACTION_BUTTON_RELEASE respectively because they provide
        // info about the changed-button.
        if (eventAction == MotionEvent.ACTION_DOWN || eventAction == MotionEvent.ACTION_UP) {
            // While we use the action buttons for the changed state it is important to still
            // consume the down/up events to get the complete stream for a drag gesture, which
            // is provided using ACTION_MOVE touch events.
            return true;
        }

        float scale = getEventSourceScaling();

        EventForwarderJni.get().onMouseEvent(mNativeEventForwarder, EventForwarder.this,
                event.getEventTime(), eventAction, event.getX() / scale, event.getY() / scale,
                event.getPointerId(0), event.getPressure(0), event.getOrientation(0),
                event.getAxisValue(MotionEvent.AXIS_TILT, 0), getMouseEventActionButton(event),
                event.getButtonState(), event.getMetaState(), event.getToolType(0));
        return true;
    }

    /**
     * Manages internal state to work around a device-specific issue. Needs to be called per
     * every mouse event to update the state.
     */
    private void updateMouseEventState(MotionEvent event) {
        int eventAction = event.getActionMasked();

        if (eventAction == MotionEvent.ACTION_BUTTON_PRESS
                || eventAction == MotionEvent.ACTION_BUTTON_RELEASE) {
            mLastMouseButtonState = event.getButtonState();
        }
    }

    @TargetApi(Build.VERSION_CODES.M)
    public static int getMouseEventActionButton(MotionEvent event) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) return event.getActionButton();

        // On <M, the only mice events sent are hover events, which cannot have a button.
        return 0;
    }

    /**
     * @see View#onDragEvent(DragEvent)
     * @param event {@link DragEvent} instance.
     * @param containerView A view on which the drag event is taking place.
     */
    @TargetApi(Build.VERSION_CODES.N)
    public boolean onDragEvent(DragEvent event, View containerView) {
        if (mNativeEventForwarder == 0 || Build.VERSION.SDK_INT <= Build.VERSION_CODES.M) {
            return false;
        }

        ClipDescription clipDescription = event.getClipDescription();

        // text/* will match text/uri-list, text/html, text/plain.
        String[] mimeTypes =
                clipDescription == null ? new String[0] : clipDescription.filterMimeTypes("text/*");

        if (event.getAction() == DragEvent.ACTION_DRAG_STARTED) {
            // TODO(hush): support dragging more than just text.
            return mimeTypes != null && mimeTypes.length > 0 && mIsDragDropEnabled;
        }

        StringBuilder content = new StringBuilder("");
        if (event.getAction() == DragEvent.ACTION_DROP) {
            // TODO(hush): obtain dragdrop permissions, when dragging files into Chrome/WebView is
            // supported. Not necessary to do so for now, because only text dragging is supported.
            ClipData clipData = event.getClipData();
            final int itemCount = clipData.getItemCount();
            for (int i = 0; i < itemCount; i++) {
                ClipData.Item item = clipData.getItemAt(i);
                content.append(item.coerceToStyledText(containerView.getContext()));
            }
        }

        int[] locationOnScreen = new int[2];
        containerView.getLocationOnScreen(locationOnScreen);

        // All coordinates are in device pixel. Conversion to DIP happens in the native.
        int x = (int) (event.getX() + mCurrentTouchOffsetX);
        int y = (int) (event.getY() + mCurrentTouchOffsetY);
        int screenX = x + locationOnScreen[0];
        int screenY = y + locationOnScreen[1];

        float scale = getEventSourceScaling();

        EventForwarderJni.get().onDragEvent(mNativeEventForwarder, EventForwarder.this,
                event.getAction(), (int) (x / scale), (int) (y / scale), (int) (screenX / scale),
                (int) (screenY / scale), mimeTypes, content.toString());
        return true;
    }

    /**
     * Forwards a gesture event.
     *
     * @param type Type of the gesture event.
     * @param timeMs Time the event occurred in milliseconds.
     * @param delta Scale factor for pinch gesture relative to the current state,
     *        1.0 being 100%. If negative, has the effect of reverting
     *        pinch scale to default.
     */
    public boolean onGestureEvent(@GestureEventType int type, long timeMs, float delta) {
        if (mNativeEventForwarder == 0) return false;
        return EventForwarderJni.get().onGestureEvent(
                mNativeEventForwarder, EventForwarder.this, type, timeMs, delta);
    }

    /**
     * @see View#onGenericMotionEvent()
     */
    public boolean onGenericMotionEvent(MotionEvent event) {
        if (mNativeEventForwarder == 0) return false;
        if ((event.getSource() & InputDevice.SOURCE_CLASS_POINTER) != 0
                && event.getToolType(0) == MotionEvent.TOOL_TYPE_MOUSE) {
            updateMouseEventState(event);
        }

        return EventForwarderJni.get().onGenericMotionEvent(
                mNativeEventForwarder, EventForwarder.this, event, event.getEventTime());
    }

    /**
     * @see View#onKeyUp()
     */
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (mNativeEventForwarder == 0) return false;
        return EventForwarderJni.get().onKeyUp(
                mNativeEventForwarder, EventForwarder.this, event, keyCode);
    }

    /**
     * @see View#dispatchKeyEvent()
     */
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (mNativeEventForwarder == 0) return false;
        return EventForwarderJni.get().dispatchKeyEvent(
                mNativeEventForwarder, EventForwarder.this, event);
    }

    /**
     * @see View#scrollBy()
     */
    public void scrollBy(float dxPix, float dyPix) {
        if (mNativeEventForwarder == 0) return;
        EventForwarderJni.get().scrollBy(mNativeEventForwarder, EventForwarder.this, dxPix, dyPix);
    }

    /**
     * @see View#scrollTo()
     */
    public void scrollTo(float xPix, float yPix) {
        if (mNativeEventForwarder == 0) return;
        EventForwarderJni.get().scrollTo(mNativeEventForwarder, EventForwarder.this, xPix, yPix);
    }

    @VisibleForTesting
    public void doubleTapForTest(long timeMs, int x, int y) {
        if (mNativeEventForwarder == 0) return;
        EventForwarderJni.get().doubleTap(mNativeEventForwarder, EventForwarder.this, timeMs, x, y);
    }

    /**
     * Flings the viewport with velocity vector (velocityX, velocityY).
     * @param timeMs the current time.
     * @param velocityX fling speed in x-axis.
     * @param velocityY fling speed in y-axis.
     * @param syntheticScroll true if generated by gamepad (which will make this fixed-velocity
     * fling)
     * @param preventBoost if false, this fling may boost an existing fling. Otherwise, ends the
     * current fling and starts a new one.
     */
    public void startFling(long timeMs, float velocityX, float velocityY, boolean syntheticScroll,
            boolean preventBoosting) {
        if (mNativeEventForwarder == 0) return;
        EventForwarderJni.get().startFling(mNativeEventForwarder, EventForwarder.this, timeMs,
                velocityX, velocityY, syntheticScroll, preventBoosting);
    }

    /**
     * Cancel any fling gestures active.
     * @param timeMs Current time (in milliseconds).
     */
    public void cancelFling(long timeMs) {
        if (mNativeEventForwarder == 0) return;
        EventForwarderJni.get().cancelFling(
                mNativeEventForwarder, EventForwarder.this, timeMs, /*preventBoosting*/ true);
    }

    @NativeMethods
    interface Natives {
        WindowAndroid getJavaWindowAndroid(long nativeEventForwarder, EventForwarder caller);
        // All touch events (including flings, scrolls etc) accept coordinates in physical pixels.
        boolean onTouchEvent(long nativeEventForwarder, EventForwarder caller, MotionEvent event,
                long timeMs, int action, int pointerCount, int historySize, int actionIndex,
                float x0, float y0, float x1, float y1, int pointerId0, int pointerId1,
                float touchMajor0, float touchMajor1, float touchMinor0, float touchMinor1,
                float orientation0, float orientation1, float tilt0, float tilt1, float rawX,
                float rawY, int androidToolType0, int androidToolType1, int androidButtonState,
                int androidMetaState, boolean isTouchHandleEvent);

        void onMouseEvent(long nativeEventForwarder, EventForwarder caller, long timeMs, int action,
                float x, float y, int pointerId, float pressure, float orientation, float tilt,
                int changedButton, int buttonState, int metaState, int toolType);
        void onDragEvent(long nativeEventForwarder, EventForwarder caller, int action, int x, int y,
                int screenX, int screenY, String[] mimeTypes, String content);
        boolean onGestureEvent(long nativeEventForwarder, EventForwarder caller, int type,
                long timeMs, float delta);
        boolean onGenericMotionEvent(
                long nativeEventForwarder, EventForwarder caller, MotionEvent event, long timeMs);
        boolean onKeyUp(
                long nativeEventForwarder, EventForwarder caller, KeyEvent event, int keyCode);
        boolean dispatchKeyEvent(long nativeEventForwarder, EventForwarder caller, KeyEvent event);
        void scrollBy(long nativeEventForwarder, EventForwarder caller, float deltaX, float deltaY);
        void scrollTo(long nativeEventForwarder, EventForwarder caller, float x, float y);
        void doubleTap(long nativeEventForwarder, EventForwarder caller, long timeMs, int x, int y);
        void startFling(long nativeEventForwarder, EventForwarder caller, long timeMs,
                float velocityX, float velocityY, boolean syntheticScroll, boolean preventBoosting);
        void cancelFling(long nativeEventForwarder, EventForwarder caller, long timeMs,
                boolean preventBoosting);
    }
}
