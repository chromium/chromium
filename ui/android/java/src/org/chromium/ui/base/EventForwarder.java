// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.ClipData;
import android.content.ClipDescription;
import android.net.Uri;
import android.os.Build;
import android.view.DragEvent;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.VelocityTracker;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContentUriUtils;
import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.input.InputFeatureMap;
import org.chromium.ui.util.MotionEventUtils;

import java.lang.reflect.UndeclaredThrowableException;
import java.util.ArrayList;
import java.util.List;

/** Class used to forward view, input events down to native. */
@JNINamespace("ui")
@NullMarked
public class EventForwarder {
    private static final String TAG = "EventForwarder";
    private final boolean mIsDragDropEnabled;
    private final boolean mConvertTrackpadEventsToMouse;
    private final boolean mUseBufferedInput;

    private final MotionEvent.PointerCoords mTmpPointerCoords = new MotionEvent.PointerCoords();

    private final boolean mIsAtLeastU =
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE;

    // The mime type for a URL.
    private static final String URL_MIME_TYPE = "text/x-moz-url";

    private long mNativeEventForwarder;

    // Offset for the events that passes through.
    private float mCurrentTouchOffsetY;

    // Offset for the drag events that's dispatching through other views.
    private float mDragDispatchingOffsetX;
    private float mDragDispatchingOffsetY;

    private int mLastMouseButtonState;

    // Track the last tool type of touch sequence.
    private int mLastToolType;

    // Tracks the starting position of the last trackpad scroll.
    // Only used when isTrackpadScrollEventFromAtLeastU() is true.
    private float mLastTrackpadScrollStartX;
    private float mLastTrackpadScrollStartY;
    private float mLastTrackpadScrollStartRawX;
    private float mLastTrackpadScrollStartRawY;
    // Tracks the position of the last trackpad scroll event including move (updates).
    // Used to compute the delta in X, Y manually.
    private float mLastTrackpadScrollX;
    private float mLastTrackpadScrollY;

    private final VelocityTracker mVelocityTracker;
    private static final float MAX_FLING_VELOCITY = 8000;
    private static final float MIN_FLING_VELOCITY = 50;

    // Delegate to call WebContents functionality.
    private @Nullable StylusWritingDelegate mStylusWritingDelegate;

    private final PointerLockEventHelper mPointerLockEventHelper = new PointerLockEventHelper();

    /** Interface to provide stylus writing functionality. */
    public interface StylusWritingDelegate {
        /**
         * Handle touch events for stylus handwriting.
         *
         * @param motionEvent the motion event to be handled.
         * @return true if the event is consumed.
         */
        boolean handleTouchEvent(MotionEvent motionEvent);

        /**
         * Handle hover events for stylus handwriting.
         *
         * @param motionEvent the motion event to be handled.
         */
        void handleHoverEvent(MotionEvent motionEvent);
    }

    public void setStylusWritingDelegate(StylusWritingDelegate stylusWritingDelegate) {
        mStylusWritingDelegate = stylusWritingDelegate;
    }

    public int getLastToolType() {
        return mLastToolType;
    }

    @CalledByNative
    private static EventForwarder create(long nativeEventForwarder, boolean isDragDropEnabled) {
        final boolean convertTrackpadEventsToMouse =
                Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE;
        final boolean useBufferedInput =
                InputFeatureMap.isEnabled(InputFeatureMap.USE_ANDROID_BUFFERED_INPUT_DISPATCH);
        return new EventForwarder(
                nativeEventForwarder,
                isDragDropEnabled,
                convertTrackpadEventsToMouse,
                useBufferedInput);
    }

    @VisibleForTesting
    EventForwarder(
            long nativeEventForwarder,
            boolean isDragDropEnabled,
            boolean convertTrackpadEventsToMouse,
            boolean useBufferedInput) {
        mNativeEventForwarder = nativeEventForwarder;
        mIsDragDropEnabled = isDragDropEnabled;
        mConvertTrackpadEventsToMouse = convertTrackpadEventsToMouse;
        mUseBufferedInput = useBufferedInput;
        mVelocityTracker = VelocityTracker.obtain();
    }

    @CalledByNative
    private void destroy() {
        mNativeEventForwarder = 0;
    }

    private boolean hasTouchEventOffset() {
        return mCurrentTouchOffsetY != 0.0f;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({
        InputDeviceSource.OTHER,
        InputDeviceSource.TOUCHSCREEN,
        InputDeviceSource.TOUCHPAD,
        InputDeviceSource.MOUSE,
        InputDeviceSource.STYLUS,
        InputDeviceSource.COUNT
    })
    private @interface InputDeviceSource {
        int OTHER = 0;
        int TOUCHSCREEN = 1;
        int TOUCHPAD = 2;
        int MOUSE = 3;
        int STYLUS = 4;
        int COUNT = 5;
    }

    private static final void logActionDown(MotionEvent event) {
        @InputDeviceSource int source = InputDeviceSource.OTHER;
        if (event.isFromSource(InputDevice.SOURCE_MOUSE)
                && event.getToolType(0) == MotionEvent.TOOL_TYPE_FINGER) {
            // On the event a touchpad is not indicated as a distinct source from a mouse, but the
            // tool type is different.
            source = InputDeviceSource.TOUCHPAD;
        } else if (event.isFromSource(InputDevice.SOURCE_MOUSE)
                && event.getToolType(0) == MotionEvent.TOOL_TYPE_MOUSE) {
            source = InputDeviceSource.MOUSE;
        } else if (event.isFromSource(InputDevice.SOURCE_STYLUS)) {
            // Check stylus before touchscreen. In the case of a stylus acting on a touchscreen both
            // will be true, but stylus is more specific.
            source = InputDeviceSource.STYLUS;
        } else if (event.isFromSource(InputDevice.SOURCE_TOUCHSCREEN)) {
            source = InputDeviceSource.TOUCHSCREEN;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "Android.Event.ActionDown", source, InputDeviceSource.COUNT);
    }

    /**
     * @see View#onTouchEvent(MotionEvent)
     */
    public boolean onTouchEvent(MotionEvent event) {
        if (event.getAction() == MotionEvent.ACTION_DOWN) {
            mLastToolType = event.getToolType(0);
            logActionDown(event);
        }

        if (touchEventRequiresSpecialHandling(event)) {
            return true;
        }

        final boolean isTouchHandleEvent = false;
        return sendTouchEvent(event, isTouchHandleEvent);
    }

    /**
     * Called by PopupWindow-based touch handles.
     *
     * @param event the MotionEvent targeting the handle.
     */
    public boolean onTouchHandleEvent(MotionEvent event) {
        final boolean isTouchHandleEvent = true;
        return sendTouchEvent(event, isTouchHandleEvent);
    }

    private boolean touchEventRequiresSpecialHandling(MotionEvent event) {
        if (mStylusWritingDelegate != null && mStylusWritingDelegate.handleTouchEvent(event)) {
            // Stylus writing system can consume the touch events once writing is started.
            return true;
        } else if (isTrackpadToMouseEventConversionEnabled()
                && isTrackpadToMouseConversionEvent(event)) {
            return onMouseEvent(event);
        } else if (isTrackpadToMouseEventConversionEnabled()
                && isTrackpadScrollEventFromAtLeastU(event)) {
            // At API level 34+, trackpad scroll events carry
            // AXIS_GESTURE_SCROLL_{X,Y}_DISTANCE information. Send such events
            // separately, which are converted to mouse wheel events later.
            //
            // Trackpad scroll events prior to API level 34 will be handled in
            // the same way as touchscreen swipe.
            onTrackpadScrollEvent(event);
            return true;
        } else if (event.getToolType(0) == MotionEvent.TOOL_TYPE_MOUSE) {
            // TODO(mustaq): Should we include MotionEvent.TOOL_TYPE_STYLUS here?
            // crbug.com/592082

            // Skip firing mouse events in the following cases:
            // - In Android L and below, where mouse button info is incomplete.
            // - A move w/o a button press, which represents a trackpad scroll. Real mouse moves w/o
            //   buttons goes to onHoverEvent.
            // TODO(mustaq): Look into the relevancy of this code path
            final boolean isTouchpadScroll =
                    event.getButtonState() == 0
                            && (event.getActionMasked() == MotionEvent.ACTION_DOWN
                                    || event.getActionMasked() == MotionEvent.ACTION_MOVE
                                    || event.getActionMasked() == MotionEvent.ACTION_UP
                                    || event.getActionMasked() == MotionEvent.ACTION_CANCEL);

            if (!isTouchpadScroll) {
                return onMouseEvent(event);
            }
        }
        return false;
    }

    @CalledByNative
    private float getWebContentsOffsetYInWindow() {
        return mCurrentTouchOffsetY;
    }

    private boolean sendTouchEvent(MotionEvent event, boolean isTouchHandleEvent) {
        assert mNativeEventForwarder != 0;

        TraceEvent.begin("sendTouchEvent");
        try {
            final int historySize = event.getHistorySize();
            // Android may batch multiple events together for efficiency. We want to use the oldest
            // event time as hardware time stamp. Unless we're using Android buffered input, in
            // which case we use MotionEvent.getEventTime[Nanos](), which the OS already resampled
            // based on historical events. Note that we still keep the historical events for
            // tracking velocity.
            final long latestEventTime = MotionEventUtils.getEventTimeNanos(event);
            final long oldestEventTime =
                    historySize == 0 || mUseBufferedInput
                            ? latestEventTime
                            : MotionEventUtils.getHistoricalEventTimeNanos(event, 0);
            final boolean isLatestEventTimeResampled;
            if (mUseBufferedInput) {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
                    event.getPointerCoords(0, mTmpPointerCoords);
                    isLatestEventTimeResampled = mTmpPointerCoords.isResampled();
                    mTmpPointerCoords.clear();
                } else {
                    isLatestEventTimeResampled = true;
                }
            } else {
                isLatestEventTimeResampled = false;
            }

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
                event.getTouchMajor(), pointerCount > 1 ? event.getTouchMajor(1) : 0
            };
            float[] touchMinor = {
                event.getTouchMinor(), pointerCount > 1 ? event.getTouchMinor(1) : 0
            };

            for (int i = 0; i < 2; i++) {
                if (touchMajor[i] < touchMinor[i]) {
                    float tmp = touchMajor[i];
                    touchMajor[i] = touchMinor[i];
                    touchMinor[i] = tmp;
                }
            }

            int gestureClassification = event.getClassification();

            final boolean consumed =
                    EventForwarderJni.get()
                            .onTouchEvent(
                                    mNativeEventForwarder,
                                    event,
                                    oldestEventTime,
                                    latestEventTime,
                                    eventAction,
                                    touchMajor[0],
                                    touchMajor[1],
                                    touchMinor[0],
                                    touchMinor[1],
                                    gestureClassification,
                                    isTouchHandleEvent,
                                    isLatestEventTimeResampled);

            if (didOffsetEvent) event.recycle();
            return consumed;
        } finally {
            TraceEvent.end("sendTouchEvent");
        }
    }

    /**
     * Sets the current amount to offset incoming touch events by (including MotionEvent and
     * DragEvent). This is used to handle content moving and not lining up properly with the android
     * input system.
     *
     * @param dy The Y offset in pixels to shift touch events.
     */
    public void setCurrentTouchOffsetY(float dy) {
        mCurrentTouchOffsetY = dy;
    }

    /**
     * Sets the current amount to offset incoming drag events by. Used for {@link DragEvent} only.
     * Usually used when dispatching drag events dispatched from views other than the ContentView.
     *
     * @param dx The X offset in pixels to shift drag events.
     * @param dy The Y offset in pixels to shift drag events.
     * @see #setCurrentTouchEventOffsets(float, float) to offset both touch and drag events.
     */
    public void setDragDispatchingOffset(float dx, float dy) {
        mDragDispatchingOffsetX = dx;
        mDragDispatchingOffsetY = dy;
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
        dst.offsetLocation(/* deltaX= */ 0, mCurrentTouchOffsetY);
        return dst;
    }

    private static boolean isValidTouchEventActionForNative(int eventAction) {
        // Only these actions have any effect on gesture detection.  Other
        // actions have no corresponding WebTouchEvent type and may confuse the
        // touch pipeline, so we ignore them entirely.
        return eventAction == MotionEvent.ACTION_DOWN
                || eventAction == MotionEvent.ACTION_UP
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

        if (mStylusWritingDelegate != null) {
            mStylusWritingDelegate.handleHoverEvent(event);
        }

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
                    EventForwarderJni.get()
                            .onMouseEvent(
                                    mNativeEventForwarder,
                                    event,
                                    MotionEventUtils.getEventTimeNanos(event),
                                    MotionEvent.ACTION_BUTTON_RELEASE,
                                    MotionEvent.BUTTON_PRIMARY,
                                    event.getToolType(0));
                }
                mLastMouseButtonState = 0;
            }
            // If trackpad scrolls are converted to mousewheel scrolls, so do touchpad flings, and
            // trackpad movements to stop fling need to be handled here too.
            if (isTrackpadToMouseEventConversionEnabled()
                    && event.isFromSource(InputDevice.SOURCE_MOUSE)
                    && event.getToolType(0) == MotionEvent.TOOL_TYPE_FINGER
                    && eventAction == MotionEvent.ACTION_HOVER_MOVE) {
                cancelFling(event.getEventTime(), true);
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
     * Sends mouse event to native. Hover event is also converted to mouse event, only
     * differentiated by an internal flag.
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
        boolean shouldConvertToMouseEvent =
                isTrackpadToMouseEventConversionEnabled()
                        && isTrackpadToMouseConversionEvent(event);

        mPointerLockEventHelper.onNonCapturedPointerEvent(event.getX(), event.getY());

        EventForwarderJni.get()
                .onMouseEvent(
                        mNativeEventForwarder,
                        event,
                        MotionEventUtils.getEventTimeNanos(event),
                        eventAction,
                        getMouseEventActionButton(event),
                        shouldConvertToMouseEvent
                                ? MotionEvent.TOOL_TYPE_MOUSE
                                : event.getToolType(0));
        return true;
    }

    private void onTrackpadScrollEvent(MotionEvent event) {
        float deltaX = 0;
        float deltaY = 0;
        // Convert trackpad scroll to mouse wheel event.
        if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
            mLastTrackpadScrollStartX = event.getX();
            mLastTrackpadScrollStartY = event.getY() + mCurrentTouchOffsetY;
            mLastTrackpadScrollStartRawX = event.getRawX();
            mLastTrackpadScrollStartRawY = event.getRawY() + mCurrentTouchOffsetY;
        } else {
            deltaX = event.getX() - mLastTrackpadScrollX;
            deltaY = event.getY() + mCurrentTouchOffsetY - mLastTrackpadScrollY;
        }
        mLastTrackpadScrollX = event.getX();
        mLastTrackpadScrollY = event.getY() + mCurrentTouchOffsetY;

        // Fling detection. Start fling at the end of scroll if the accumulated velocity is higher
        // than the threshold. If this happens, return early and do not send the UP event which will
        // be converted to a scroll end event since fling will end the scroll sequence.
        mVelocityTracker.addMovement(event);
        if (event.getActionMasked() == MotionEvent.ACTION_UP) {
            mVelocityTracker.computeCurrentVelocity(/* units= */ 1000, MAX_FLING_VELOCITY);
            float velocityX = mVelocityTracker.getXVelocity();
            float velocityY = mVelocityTracker.getYVelocity();
            if (Math.abs(velocityX) > MIN_FLING_VELOCITY
                    || Math.abs(velocityY) > MIN_FLING_VELOCITY) {
                startFling(event.getEventTime(), velocityX, velocityY, false, false, true);
                return;
            }
        }

        // New two-finger movements should stop any on-going fling.
        if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
            cancelFling(event.getEventTime(), true);
        }

        EventForwarderJni.get()
                .onMouseWheelEvent(
                        mNativeEventForwarder,
                        event,
                        MotionEventUtils.getEventTimeNanos(event),
                        mLastTrackpadScrollStartX,
                        mLastTrackpadScrollStartY,
                        mLastTrackpadScrollStartRawX,
                        mLastTrackpadScrollStartRawY,
                        deltaX,
                        deltaY);
    }

    /**
     * Manages internal state to work around a device-specific issue. Needs to be called per every
     * mouse event to update the state.
     */
    private void updateMouseEventState(MotionEvent event) {
        int eventAction = event.getActionMasked();

        if (eventAction == MotionEvent.ACTION_BUTTON_PRESS
                || eventAction == MotionEvent.ACTION_BUTTON_RELEASE) {
            mLastMouseButtonState = event.getButtonState();
        }
    }

    public static int getMouseEventActionButton(MotionEvent event) {
        return event.getActionButton();
    }

    public boolean isTrackpadToMouseEventConversionEnabled() {
        return mConvertTrackpadEventsToMouse;
    }

    /**
     * Returns true if a {@link MotionEvent} is a trackpad event that should be converted to mouse
     * event, including: click, click-and-drag, hover event. Returns true for button release events
     * as well.
     */
    public static boolean isTrackpadToMouseConversionEvent(MotionEvent event) {
        if (MotionEventUtils.isTrackpadEvent(event)) {
            // Click or click-and-drag.
            if (event.getAction() == MotionEvent.ACTION_BUTTON_RELEASE
                    || event.getButtonState() != 0) {
                return true;
            }

            // Hover.
            if (event.getAction() == MotionEvent.ACTION_HOVER_MOVE) {
                return true;
            }
        }

        return false;
    }

    /** Only supports API level 34+. */
    public boolean isTrackpadScrollEventFromAtLeastU(MotionEvent event) {
        return mIsAtLeastU
                && event.getClassification() == MotionEvent.CLASSIFICATION_TWO_FINGER_SWIPE
                && (event.getActionMasked() == MotionEvent.ACTION_DOWN
                        || event.getActionMasked() == MotionEvent.ACTION_MOVE
                        || event.getActionMasked() == MotionEvent.ACTION_UP
                        || event.getActionMasked() == MotionEvent.ACTION_CANCEL);
    }

    /**
     * @see View#onDragEvent(DragEvent)
     * @param event {@link DragEvent} instance.
     * @param containerView A view on which the drag event is taking place.
     */
    public boolean onDragEvent(DragEvent event, View containerView) {
        ClipDescription clipDescription = event.getClipDescription();
        // Do not forward browser content events to native eventForwarder.
        if (MimeTypeUtils.clipDescriptionHasBrowserContent(clipDescription)) {
            return false;
        }
        if (mNativeEventForwarder == 0) {
            return false;
        }
        String[] mimeTypes =
                new String[clipDescription != null ? clipDescription.getMimeTypeCount() : 0];
        for (int i = 0; i < mimeTypes.length; i++) {
            mimeTypes[i] = clipDescription.getMimeType(i);
        }

        if (event.getAction() == DragEvent.ACTION_DRAG_STARTED) {
            return mIsDragDropEnabled;
        }

        String content = "";
        List<String[]> filenames = new ArrayList<String[]>();
        String text = null;
        String html = null;
        String url = null;
        if (event.getAction() == DragEvent.ACTION_DROP) {
            try {
                ClipData clipData = event.getClipData();
                final int itemCount = clipData == null ? 0 : clipData.getItemCount();
                for (int i = 0; i < itemCount; i++) {
                    // If there are any Uris, set them as files.
                    Uri uri = clipData.getItemAt(i).getUri();
                    if (uri != null) {
                        String uriString = uri.toString();
                        String displayName = ContentUriUtils.maybeGetDisplayName(uriString);
                        if (displayName == null) {
                            displayName = new String();
                        }
                        filenames.add(new String[] {uriString, displayName});
                    }
                }

                // Only read text, html, url if there are no Uris (files).
                if (filenames.isEmpty() && itemCount > 0) {
                    ClipData.Item item = clipData.getItemAt(0);
                    CharSequence temp = item.getText();
                    if (temp != null) {
                        text = temp.toString();
                        if (clipDescription.hasMimeType(URL_MIME_TYPE)) {
                            url = text;
                        }
                    }
                    temp = item.getHtmlText();
                    if (temp != null) {
                        html = temp.toString();
                    }
                }
                content = "";
            } catch (UndeclaredThrowableException e) {
                // When dropped item is not successful for whatever reason, catch before we crash.
                // While ClipData.Item does capture most common failures, there could be exceptions
                // that's wrapped by Chrome classes (e.g. ServiceTracingProxyProvider) which changed
                // the exception signiture. See crbug.com/1406777.
                Log.e(TAG, "Parsing clip data content failed.", e);
                content = "";
            }
            RecordHistogram.recordCount100Histogram(
                    "Android.DragDrop.Files.Count", filenames.size());
        }

        int[] locationOnScreen = new int[2];
        containerView.getLocationOnScreen(locationOnScreen);

        // All coordinates are in device pixel. Conversion to DIP happens in the native.
        float x = event.getX() + mDragDispatchingOffsetX;
        float y = event.getY() + mCurrentTouchOffsetY + mDragDispatchingOffsetY;
        float screenX = x + locationOnScreen[0];
        float screenY = y + locationOnScreen[1];

        EventForwarderJni.get()
                .onDragEvent(
                        mNativeEventForwarder,
                        event.getAction(),
                        x,
                        y,
                        screenX,
                        screenY,
                        mimeTypes,
                        content,
                        filenames.toArray(new String[][] {}),
                        text,
                        html,
                        url);
        return true;
    }

    /**
     * Forwards a gesture event.
     *
     * @param type Type of the gesture event.
     * @param timeMs Time the event occurred in milliseconds.
     * @param delta Scale factor for pinch gesture relative to the current state, 1.0 being 100%. If
     *     negative, has the effect of reverting pinch scale to default.
     */
    public boolean onGestureEvent(@GestureEventType int type, long timeMs, float delta) {
        if (mNativeEventForwarder == 0) return false;
        return EventForwarderJni.get().onGestureEvent(mNativeEventForwarder, type, timeMs, delta);
    }

    /**
     * @see View#onGenericMotionEvent()
     */
    public boolean onGenericMotionEvent(MotionEvent event) {
        if (mNativeEventForwarder == 0) return false;
        boolean isMouseEvent =
                (event.getSource() & InputDevice.SOURCE_CLASS_POINTER) != 0
                        && event.getToolType(0) == MotionEvent.TOOL_TYPE_MOUSE;
        boolean shouldConvertToMouseEvent =
                isTrackpadToMouseEventConversionEnabled()
                        && isTrackpadToMouseConversionEvent(event);
        if (isMouseEvent || shouldConvertToMouseEvent) {
            updateMouseEventState(event);
        }

        if (event.getActionMasked() == MotionEvent.ACTION_SCROLL) {
            event = createOffsetMotionEventIfNeeded(event);
        }

        return EventForwarderJni.get()
                .onGenericMotionEvent(
                        mNativeEventForwarder,
                        event,
                        MotionEventUtils.getEventTimeNanos(event),
                        event.getDownTime());
    }

    /**
     * Forwards the captured pointer events to native, transforms the captured pointer event first
     * to a format similar to the non-captured event.
     *
     * @param event, generated motion event
     * @param deviceRotation, The current device rotation, which is needed to update the captured
     *     raw touchpad events based on the device orientation
     */
    @VisibleForTesting
    public boolean onCapturedPointerEvent(MotionEvent event, int deviceRotation) {
        boolean shouldConvertToMouseEvent =
                isTrackpadToMouseEventConversionEnabled()
                        && event.isFromSource(InputDevice.SOURCE_TOUCHPAD);
        event = mPointerLockEventHelper.transformCapturedPointerEvent(event, deviceRotation);

        if (!event.isFromSource(InputDevice.SOURCE_MOUSE)) {
            Log.w(
                    TAG,
                    "Received a captured pointer event with an unexpected source %d.",
                    event.getSource());
            return true;
        }

        // For mousedown and mouseup events, we use ACTION_BUTTON_PRESS
        // and ACTION_BUTTON_RELEASE respectively because they provide
        // info about the changed-button.
        if (event.getAction() == MotionEvent.ACTION_DOWN
                || event.getAction() == MotionEvent.ACTION_UP
                || event.getActionMasked() == MotionEvent.ACTION_POINTER_DOWN
                || event.getActionMasked() == MotionEvent.ACTION_POINTER_UP) {
            // While we use the action buttons for the changed state it is important to still
            // consume the down/up events to get the complete stream for a drag gesture, which
            // is provided using ACTION_MOVE touch events.
            return true;
        }

        if (event.getX() == mPointerLockEventHelper.getLastPointerPositionX()
                && event.getY() == mPointerLockEventHelper.getLastPointerPositionY()
                && event.getAction() == MotionEvent.ACTION_MOVE) {
            // No change compared to previous event, no need to forward the event
            return true;
        }

        // Update the last event position
        mPointerLockEventHelper.updateLastPointerPosition(event.getX(), event.getY());

        if (event.getAction() == MotionEvent.ACTION_SCROLL) {
            return EventForwarderJni.get()
                    .onGenericMotionEvent(
                            mNativeEventForwarder,
                            event,
                            MotionEventUtils.getEventTimeNanos(event),
                            event.getDownTime());
        } else {
            EventForwarderJni.get()
                    .onMouseEvent(
                            mNativeEventForwarder,
                            event,
                            MotionEventUtils.getEventTimeNanos(event),
                            event.getActionMasked(),
                            getMouseEventActionButton(event),
                            shouldConvertToMouseEvent
                                    ? MotionEvent.TOOL_TYPE_MOUSE
                                    : event.getToolType(0));
        }

        return true;
    }

    /**
     * @see View#onKeyUp(), except it doesn't take keyCode as a parameter.
     */
    public boolean onKeyUp(KeyEvent event) {
        if (mNativeEventForwarder == 0) return false;
        return EventForwarderJni.get().onKeyUp(mNativeEventForwarder, event);
    }

    /**
     * @see View#dispatchKeyEvent()
     */
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (mNativeEventForwarder == 0) return false;
        return EventForwarderJni.get().dispatchKeyEvent(mNativeEventForwarder, event);
    }

    /**
     * @see View#scrollBy()
     */
    public void scrollBy(float dxPix, float dyPix) {
        if (mNativeEventForwarder == 0) return;
        EventForwarderJni.get().scrollBy(mNativeEventForwarder, dxPix, dyPix);
    }

    /**
     * @see View#scrollTo()
     */
    public void scrollTo(float xPix, float yPix) {
        if (mNativeEventForwarder == 0) return;
        EventForwarderJni.get().scrollTo(mNativeEventForwarder, xPix, yPix);
    }

    public void doubleTapForTest(long timeMs, int x, int y) {
        if (mNativeEventForwarder == 0) return;
        EventForwarderJni.get().doubleTap(mNativeEventForwarder, timeMs, x, y);
    }

    /**
     * Flings the viewport with velocity vector (velocityX, velocityY).
     *
     * @param timeMs the current time.
     * @param velocityX fling speed in x-axis.
     * @param velocityY fling speed in y-axis.
     * @param syntheticScroll true if generated by gamepad (which will make this fixed-velocity
     *     fling)
     * @param preventBoosting if false, this fling may boost an existing fling. Otherwise, ends the
     *     current fling and starts a new one.
     * @param isTouchpadEvent if true, the gesture event created will have source touchpad,
     *     touchscreen otherwise.
     */
    public void startFling(
            long timeMs,
            float velocityX,
            float velocityY,
            boolean syntheticScroll,
            boolean preventBoosting,
            boolean isTouchpadEvent) {
        if (mNativeEventForwarder == 0) return;
        EventForwarderJni.get()
                .startFling(
                        mNativeEventForwarder,
                        timeMs,
                        velocityX,
                        velocityY,
                        syntheticScroll,
                        preventBoosting,
                        isTouchpadEvent);
    }

    /**
     * Cancel any fling gestures active.
     *
     * @param timeMs Current time (in milliseconds).
     * @param isTouchpadEvent if true, the gesture event created will have source touchpad,
     *     touchscreen otherwise.
     */
    public void cancelFling(long timeMs, boolean isTouchpadEvent) {
        if (mNativeEventForwarder == 0) return;
        EventForwarderJni.get()
                .cancelFling(
                        mNativeEventForwarder,
                        timeMs,
                        /* preventBoosting= */ true,
                        isTouchpadEvent);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    interface Natives {
        // All touch events (including flings, scrolls etc) accept coordinates in physical pixels.
        boolean onTouchEvent(
                long nativeEventForwarder,
                MotionEvent event,
                long oldestEventTimeNs,
                long latestEventTimeNs,
                int action,
                float touchMajor0,
                float touchMajor1,
                float touchMinor0,
                float touchMinor1,
                int gestureClassification,
                boolean isTouchHandleEvent,
                boolean isLatestEventTimeResampled);

        void onMouseEvent(
                long nativeEventForwarder,
                MotionEvent event,
                long timeNs,
                int action,
                int changedButton,
                int toolType);

        void onDragEvent(
                long nativeEventForwarder,
                int action,
                float x,
                float y,
                float screenX,
                float screenY,
                String[] mimeTypes,
                String content,
                String[][] filenames,
                @Nullable String text,
                @Nullable String html,
                @Nullable String url);

        boolean onGestureEvent(long nativeEventForwarder, int type, long timeMs, float delta);

        boolean onGenericMotionEvent(
                long nativeEventForwarder, MotionEvent event, long timeNs, long downTimeMs);

        void onMouseWheelEvent(
                long nativeEventForwarder,
                MotionEvent event,
                long timeNs,
                float x,
                float y,
                float rawX,
                float rawY,
                float deltaX,
                float deltaY);

        boolean onKeyUp(long nativeEventForwarder, @JniType("ui::KeyEventAndroid") KeyEvent event);

        boolean dispatchKeyEvent(
                long nativeEventForwarder, @JniType("ui::KeyEventAndroid") KeyEvent event);

        void scrollBy(long nativeEventForwarder, float deltaX, float deltaY);

        void scrollTo(long nativeEventForwarder, float x, float y);

        void doubleTap(long nativeEventForwarder, long timeMs, int x, int y);

        void startFling(
                long nativeEventForwarder,
                long timeMs,
                float velocityX,
                float velocityY,
                boolean syntheticScroll,
                boolean preventBoosting,
                boolean isTouchpadEvent);

        void cancelFling(
                long nativeEventForwarder,
                long timeMs,
                boolean preventBoosting,
                boolean isTouchpadEvent);
    }
}
