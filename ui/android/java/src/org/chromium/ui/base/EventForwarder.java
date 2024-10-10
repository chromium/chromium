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
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContentUriUtils;
import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.ui.MotionEventUtils;

import java.lang.reflect.UndeclaredThrowableException;
import java.util.ArrayList;
import java.util.List;

/** Class used to forward view, input events down to native. */
@JNINamespace("ui")
public class EventForwarder {
    private static final String TAG = "EventForwarder";
    private final boolean mIsDragDropEnabled;
    private final boolean mConvertTrackpadEventsToMouse;

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

    // Delegate to call WebContents functionality.
    private StylusWritingDelegate mStylusWritingDelegate;

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
        final boolean isAtLeastU = Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE;
        final boolean convertTrackpadEventsToMouse =
                isAtLeastU
                        && UiAndroidFeatureMap.isEnabled(
                                UiAndroidFeatures.CONVERT_TRACKPAD_EVENTS_TO_MOUSE);
        return new EventForwarder(
                nativeEventForwarder, isDragDropEnabled, convertTrackpadEventsToMouse);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    EventForwarder(
            long nativeEventForwarder,
            boolean isDragDropEnabled,
            boolean convertTrackpadEventsToMouse) {
        mNativeEventForwarder = nativeEventForwarder;
        mIsDragDropEnabled = isDragDropEnabled;
        mConvertTrackpadEventsToMouse = convertTrackpadEventsToMouse;
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
                && isTrackpadClickOrClickAndDragEvent(event)) {
            return onMouseEvent(event);
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

    private boolean sendTouchEvent(MotionEvent event, boolean isTouchHandleEvent) {
        assert mNativeEventForwarder != 0;

        TraceEvent.begin("sendTouchEvent");
        try {
            final int historySize = event.getHistorySize();
            // Android may batch multiple events together for efficiency. We
            // want to use the oldest event time as hardware time stamp.
            final long latestEventTime = MotionEventUtils.getEventTimeNanos(event);
            final long oldestEventTime =
                    historySize == 0
                            ? latestEventTime
                            : MotionEventUtils.getHistoricalEventTimeNanos(event, 0);

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

            float secondPointerX = pointerCount > 1 ? event.getX(1) : 0;
            float secondPointerY = pointerCount > 1 ? event.getY(1) : 0;

            int gestureClassification = 0;
            if (Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.Q) {
                gestureClassification = event.getClassification();
            }

            final boolean consumed =
                    EventForwarderJni.get()
                            .onTouchEvent(
                                    mNativeEventForwarder,
                                    EventForwarder.this,
                                    event,
                                    oldestEventTime,
                                    latestEventTime,
                                    eventAction,
                                    pointerCount,
                                    historySize,
                                    event.getActionIndex(),
                                    event.getX(),
                                    event.getY(),
                                    secondPointerX,
                                    secondPointerY,
                                    event.getPointerId(0),
                                    pointerCount > 1 ? event.getPointerId(1) : -1,
                                    touchMajor[0],
                                    touchMajor[1],
                                    touchMinor[0],
                                    touchMinor[1],
                                    event.getOrientation(),
                                    pointerCount > 1 ? event.getOrientation(1) : 0,
                                    event.getAxisValue(MotionEvent.AXIS_TILT),
                                    pointerCount > 1
                                            ? event.getAxisValue(MotionEvent.AXIS_TILT, 1)
                                            : 0,
                                    event.getRawX(),
                                    event.getRawY(),
                                    event.getToolType(0),
                                    pointerCount > 1
                                            ? event.getToolType(1)
                                            : MotionEvent.TOOL_TYPE_UNKNOWN,
                                    gestureClassification,
                                    event.getButtonState(),
                                    event.getMetaState(),
                                    isTouchHandleEvent);

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
                                    EventForwarder.this,
                                    MotionEventUtils.getEventTimeNanos(event),
                                    MotionEvent.ACTION_BUTTON_RELEASE,
                                    event.getX(),
                                    event.getY(),
                                    event.getPointerId(0),
                                    event.getPressure(0),
                                    event.getOrientation(0),
                                    event.getAxisValue(MotionEvent.AXIS_TILT, 0),
                                    MotionEvent.BUTTON_PRIMARY,
                                    event.getButtonState(),
                                    event.getMetaState(),
                                    event.getToolType(0));
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
                        && isTrackpadClickOrClickAndDragEvent(event);
        EventForwarderJni.get()
                .onMouseEvent(
                        mNativeEventForwarder,
                        EventForwarder.this,
                        MotionEventUtils.getEventTimeNanos(event),
                        eventAction,
                        event.getX(),
                        event.getY(),
                        event.getPointerId(0),
                        event.getPressure(0),
                        event.getOrientation(0),
                        event.getAxisValue(MotionEvent.AXIS_TILT, 0),
                        getMouseEventActionButton(event),
                        event.getButtonState(),
                        event.getMetaState(),
                        shouldConvertToMouseEvent
                                ? MotionEvent.TOOL_TYPE_MOUSE
                                : event.getToolType(0));
        return true;
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
     * Returns true if a {@link MotionEvent} is a trackpad click and or click & drag event.
     * Trackpad hover events and non-click gestures (i.e two-finger scroll) should return
     * false here as they do have an action button pressed. Also we want to make sure we
     * return true for button release events as well.
     */
    public static boolean isTrackpadClickOrClickAndDragEvent(MotionEvent event) {
        return isTrackpadEvent(event)
                && (event.getAction() == MotionEvent.ACTION_BUTTON_RELEASE
                        || event.getButtonState() != 0);
    }

    /**
     * Returns true if a {@link MotionEvent} is detected to be a trackpad event.
     * Note that {@link MotionEvent.TOOL_TYPE_FINGER} is used here along with
     * {@link InputDevice.SOURCE_MOUSE} instead of {@link InputDevice.SOURCE_TOUCHPAD}
     * because {@link InputDevice.SOURCE_TOUCHPAD} is used when an app
     * captures the touchpad meaning that it gets access to the raw finger locations,
     * dimensions etc. reported by the touchpad rather than those being used for pointer movements
     * and gestures.
     */
    private static boolean isTrackpadEvent(MotionEvent event) {
        return event.isFromSource(InputDevice.SOURCE_MOUSE)
                && event.getToolType(0) == MotionEvent.TOOL_TYPE_FINGER;
    }

    /**
     * @see View#onDragEvent(DragEvent)
     * @param event {@link DragEvent} instance.
     * @param containerView A view on which the drag event is taking place.
     */
    public boolean onDragEvent(DragEvent event, View containerView) {
        ClipDescription clipDescription = event.getClipDescription();
        // Do not forward chrome/tab events to native eventForwarder.
        if (clipDescription != null
                && clipDescription.hasMimeType(MimeTypeUtils.CHROME_MIMETYPE_TAB)) {
            return false;
        }
        if (mNativeEventForwarder == 0) {
            return false;
        }
        boolean dragDropFilesEnabled =
                UiAndroidFeatureMap.isEnabled(UiAndroidFeatureList.DRAG_DROP_FILES);
        String[] mimeTypes = null;
        if (dragDropFilesEnabled) {
            mimeTypes =
                    new String[clipDescription != null ? clipDescription.getMimeTypeCount() : 0];
            for (int i = 0; i < mimeTypes.length; i++) {
                mimeTypes[i] = clipDescription.getMimeType(i);
            }
        } else {
            // text/* will match text/uri-list, text/html, text/plain.
            mimeTypes =
                    clipDescription == null
                            ? new String[0]
                            : clipDescription.filterMimeTypes("text/*");
            // mimeTypes is null iff there is no matching text MIME type.
            // Try if there is any matching image MIME type.
            if (mimeTypes == null) {
                mimeTypes = clipDescription.filterMimeTypes("image/*");
            }
        }

        if (event.getAction() == DragEvent.ACTION_DRAG_STARTED) {
            return mIsDragDropEnabled
                    && ((mimeTypes != null && mimeTypes.length > 0)
                            || UiAndroidFeatureMap.isEnabled(UiAndroidFeatureList.DRAG_DROP_EMPTY));
        }

        String content = "";
        List<String[]> filenames = new ArrayList<String[]>();
        String text = null;
        String html = null;
        String url = null;
        if (event.getAction() == DragEvent.ACTION_DROP) {
            try {
                StringBuilder contentBuilder = new StringBuilder("");
                ClipData clipData = event.getClipData();
                final int itemCount = clipData == null ? 0 : clipData.getItemCount();
                for (int i = 0; i < itemCount; i++) {
                    if (!dragDropFilesEnabled) {
                        ClipData.Item item = clipData.getItemAt(i);
                        contentBuilder.append(item.coerceToStyledText(containerView.getContext()));
                        continue;
                    }

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
                if (dragDropFilesEnabled && filenames.isEmpty() && itemCount > 0) {
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
                content = contentBuilder.toString();
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
                        EventForwarder.this,
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
        return EventForwarderJni.get()
                .onGestureEvent(mNativeEventForwarder, EventForwarder.this, type, timeMs, delta);
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
                        && isTrackpadClickOrClickAndDragEvent(event);
        if (isMouseEvent || shouldConvertToMouseEvent) {
            updateMouseEventState(event);
        }

        if (event.getActionMasked() == MotionEvent.ACTION_SCROLL) {
            event = createOffsetMotionEventIfNeeded(event);
        }

        return EventForwarderJni.get()
                .onGenericMotionEvent(
                        mNativeEventForwarder,
                        EventForwarder.this,
                        event,
                        MotionEventUtils.getEventTimeNanos(event));
    }

    /**
     * @see View#onKeyUp()
     */
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (mNativeEventForwarder == 0) return false;
        return EventForwarderJni.get()
                .onKeyUp(mNativeEventForwarder, EventForwarder.this, event, keyCode);
    }

    /**
     * @see View#dispatchKeyEvent()
     */
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (mNativeEventForwarder == 0) return false;
        return EventForwarderJni.get()
                .dispatchKeyEvent(mNativeEventForwarder, EventForwarder.this, event);
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

    public void doubleTapForTest(long timeMs, int x, int y) {
        if (mNativeEventForwarder == 0) return;
        EventForwarderJni.get().doubleTap(mNativeEventForwarder, EventForwarder.this, timeMs, x, y);
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
     */
    public void startFling(
            long timeMs,
            float velocityX,
            float velocityY,
            boolean syntheticScroll,
            boolean preventBoosting) {
        if (mNativeEventForwarder == 0) return;
        EventForwarderJni.get()
                .startFling(
                        mNativeEventForwarder,
                        EventForwarder.this,
                        timeMs,
                        velocityX,
                        velocityY,
                        syntheticScroll,
                        preventBoosting);
    }

    /**
     * Cancel any fling gestures active.
     *
     * @param timeMs Current time (in milliseconds).
     */
    public void cancelFling(long timeMs) {
        if (mNativeEventForwarder == 0) return;
        EventForwarderJni.get()
                .cancelFling(
                        mNativeEventForwarder,
                        EventForwarder.this,
                        timeMs,
                        /* preventBoosting= */ true);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    interface Natives {
        // All touch events (including flings, scrolls etc) accept coordinates in physical pixels.
        boolean onTouchEvent(
                long nativeEventForwarder,
                EventForwarder caller,
                MotionEvent event,
                long oldestEventTimeNs,
                long latestEventTimeNs,
                int action,
                int pointerCount,
                int historySize,
                int actionIndex,
                float x0,
                float y0,
                float x1,
                float y1,
                int pointerId0,
                int pointerId1,
                float touchMajor0,
                float touchMajor1,
                float touchMinor0,
                float touchMinor1,
                float orientation0,
                float orientation1,
                float tilt0,
                float tilt1,
                float rawX,
                float rawY,
                int androidToolType0,
                int androidToolType1,
                int gestureClassification,
                int androidButtonState,
                int androidMetaState,
                boolean isTouchHandleEvent);

        void onMouseEvent(
                long nativeEventForwarder,
                EventForwarder caller,
                long timeNs,
                int action,
                float x,
                float y,
                int pointerId,
                float pressure,
                float orientation,
                float tilt,
                int changedButton,
                int buttonState,
                int metaState,
                int toolType);

        void onDragEvent(
                long nativeEventForwarder,
                EventForwarder caller,
                int action,
                float x,
                float y,
                float screenX,
                float screenY,
                String[] mimeTypes,
                String content,
                String[][] filenames,
                String text,
                String html,
                String url);

        boolean onGestureEvent(
                long nativeEventForwarder,
                EventForwarder caller,
                int type,
                long timeMs,
                float delta);

        boolean onGenericMotionEvent(
                long nativeEventForwarder, EventForwarder caller, MotionEvent event, long timeNs);

        boolean onKeyUp(
                long nativeEventForwarder, EventForwarder caller, KeyEvent event, int keyCode);

        boolean dispatchKeyEvent(long nativeEventForwarder, EventForwarder caller, KeyEvent event);

        void scrollBy(long nativeEventForwarder, EventForwarder caller, float deltaX, float deltaY);

        void scrollTo(long nativeEventForwarder, EventForwarder caller, float x, float y);

        void doubleTap(long nativeEventForwarder, EventForwarder caller, long timeMs, int x, int y);

        void startFling(
                long nativeEventForwarder,
                EventForwarder caller,
                long timeMs,
                float velocityX,
                float velocityY,
                boolean syntheticScroll,
                boolean preventBoosting);

        void cancelFling(
                long nativeEventForwarder,
                EventForwarder caller,
                long timeMs,
                boolean preventBoosting);
    }
}
