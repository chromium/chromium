// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.view.InputDevice;
import android.view.MotionEvent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.ui.MotionEventUtils;

/** Tests logic in the {@link EventForwarder} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class EventForwarderTest {
    @Rule public JniMocker mocker = new JniMocker();

    @Mock EventForwarder.Natives mNativeMock;

    private static final long NATIVE_EVENT_FORWARDER_ID = 1;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(EventForwarderJni.TEST_HOOKS, mNativeMock);
    }

    @Test
    public void testSendTrackpadClicksAsMouseEventToNative() {
        EventForwarder eventForwarder = new EventForwarder(NATIVE_EVENT_FORWARDER_ID, true, true);

        // Left click
        MotionEvent leftClickEvent = getTrackpadLeftClickEvent();
        eventForwarder.onTouchEvent(leftClickEvent);
        verifyNativeMouseEventSent(NATIVE_EVENT_FORWARDER_ID, leftClickEvent, eventForwarder, 1);

        // Right click
        MotionEvent rightClickEvent = getTrackRightClickEvent();
        eventForwarder.onTouchEvent(rightClickEvent);
        verifyNativeMouseEventSent(NATIVE_EVENT_FORWARDER_ID, rightClickEvent, eventForwarder, 1);
    }

    @Test
    public void testSendTrackpadClickReleaseAsMouseEventToNative() {
        EventForwarder eventForwarder = new EventForwarder(NATIVE_EVENT_FORWARDER_ID, true, true);

        // Left click
        MotionEvent leftClickReleaseEvent =
                getTrackpadEvent(MotionEvent.ACTION_BUTTON_RELEASE, MotionEvent.BUTTON_PRIMARY);
        eventForwarder.onTouchEvent(leftClickReleaseEvent);
        verifyNativeMouseEventSent(
                NATIVE_EVENT_FORWARDER_ID, leftClickReleaseEvent, eventForwarder, 1);

        // Right click
        MotionEvent rightClickReleaseEvent =
                getTrackpadEvent(MotionEvent.ACTION_BUTTON_RELEASE, MotionEvent.BUTTON_SECONDARY);
        eventForwarder.onTouchEvent(rightClickReleaseEvent);
        verifyNativeMouseEventSent(
                NATIVE_EVENT_FORWARDER_ID, rightClickReleaseEvent, eventForwarder, 1);
    }

    @Test
    public void testSendTrackpadClickAndDragAsMouseEventToNative() {
        EventForwarder eventForwarder = new EventForwarder(NATIVE_EVENT_FORWARDER_ID, true, true);
        MotionEvent clickAndDragEvent =
                getTrackpadEvent(MotionEvent.ACTION_MOVE, MotionEvent.BUTTON_PRIMARY);
        eventForwarder.onTouchEvent(clickAndDragEvent);
        verifyNativeMouseEventSent(NATIVE_EVENT_FORWARDER_ID, clickAndDragEvent, eventForwarder, 1);
    }

    @Test
    public void testSendTrackEventAsTouchEventWhenButtonIsNotClicked() {
        EventForwarder eventForwarder = new EventForwarder(NATIVE_EVENT_FORWARDER_ID, true, true);
        MotionEvent trackpadTouchDownEventNoClick = getTrackpadTouchDownEventNoClick();
        eventForwarder.onTouchEvent(trackpadTouchDownEventNoClick);
        verify(mNativeMock, times(1))
                .onTouchEvent(
                        anyLong(),
                        any(EventForwarder.class),
                        any(MotionEvent.class),
                        anyLong(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyInt(),
                        anyInt(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyBoolean());
        verify(mNativeMock, never())
                .onMouseEvent(
                        anyLong(),
                        any(EventForwarder.class),
                        anyLong(),
                        anyInt(),
                        anyFloat(),
                        anyFloat(),
                        anyInt(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt());
    }

    @Test
    public void testNotSendTrackpadClickAsMouseEventWhenFeatureDisabled() {
        EventForwarder eventForwarder = new EventForwarder(NATIVE_EVENT_FORWARDER_ID, true, false);
        MotionEvent trackpadClickDownEvent = getTrackpadLeftClickEvent();
        eventForwarder.onTouchEvent(trackpadClickDownEvent);
        verify(mNativeMock, never())
                .onMouseEvent(
                        anyLong(),
                        any(EventForwarder.class),
                        anyLong(),
                        anyInt(),
                        anyFloat(),
                        anyFloat(),
                        anyInt(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt());
    }

    private void verifyNativeMouseEventSent(
            long nativeEventForwarder,
            MotionEvent event,
            EventForwarder eventForwarder,
            int times) {
        verify(mNativeMock, times(times))
                .onMouseEvent(
                        nativeEventForwarder,
                        eventForwarder,
                        MotionEventUtils.getEventTimeNanos(event),
                        event.getActionMasked(),
                        event.getX(),
                        event.getY(),
                        event.getPointerId(0),
                        event.getPressure(0),
                        event.getOrientation(0),
                        event.getAxisValue(MotionEvent.AXIS_TILT, 0),
                        EventForwarder.getMouseEventActionButton(event),
                        event.getButtonState(),
                        event.getMetaState(),
                        MotionEvent.TOOL_TYPE_MOUSE);
    }

    private static MotionEvent getTrackpadTouchDownEventNoClick() {
        return getTrackpadEvent(MotionEvent.ACTION_DOWN, 0);
    }

    private static MotionEvent getTrackpadLeftClickEvent() {
        return getTrackpadEvent(MotionEvent.ACTION_BUTTON_PRESS, MotionEvent.BUTTON_PRIMARY);
    }

    private static MotionEvent getTrackRightClickEvent() {
        return getTrackpadEvent(MotionEvent.ACTION_BUTTON_PRESS, MotionEvent.BUTTON_SECONDARY);
    }

    private static MotionEvent getTrackpadEvent(int action, int buttonState) {
        return MotionEvent.obtain(
                0,
                0,
                action,
                1,
                getToolTypeFingerProperties(),
                getPointerCoords(),
                0,
                buttonState,
                0,
                0,
                0,
                0,
                getTrackpadSource(),
                0);
    }

    private static MotionEvent.PointerProperties[] getToolTypeFingerProperties() {
        MotionEvent.PointerProperties[] pointerPropertiesArray =
                new MotionEvent.PointerProperties[1];
        MotionEvent.PointerProperties trackpadProperties = new MotionEvent.PointerProperties();
        trackpadProperties.id = 7;
        trackpadProperties.toolType = MotionEvent.TOOL_TYPE_FINGER;
        pointerPropertiesArray[0] = trackpadProperties;
        return pointerPropertiesArray;
    }

    private static MotionEvent.PointerCoords[] getPointerCoords() {
        MotionEvent.PointerCoords[] pointerCoordsArray = new MotionEvent.PointerCoords[1];
        MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
        coords.x = 14;
        coords.y = 21;
        pointerCoordsArray[0] = coords;
        return pointerCoordsArray;
    }

    private static int getTrackpadSource() {
        return InputDevice.SOURCE_MOUSE;
    }
}
