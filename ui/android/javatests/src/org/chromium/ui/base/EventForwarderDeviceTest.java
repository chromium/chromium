// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.os.Build;
import android.view.InputDevice;
import android.view.MotionEvent;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.ui.MotionEventUtils;

/**
 * Tests logic in the {@link EventForwarder} class.
 *
 * <p>Most of the EventForwarder test cases are in Robolectric JUnit tests. However, somehow
 * MotionEvent.obtain() doesn't set the classification field properly in Robolectric tests.
 * Therefore this test class is added to test those cases where classification is needed.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class EventForwarderDeviceTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mocker = new JniMocker();

    @Mock EventForwarder.Natives mNativeMock;

    private static final long NATIVE_EVENT_FORWARDER_ID = 1;

    @Before
    public void setUp() {
        mocker.mock(EventForwarderJni.TEST_HOOKS, mNativeMock);
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testSendTrackpadScrollAsMouseWheelToNativeAtLeastU() {
        EventForwarder eventForwarder = new EventForwarder(NATIVE_EVENT_FORWARDER_ID, true, true);
        MotionEvent startEvent =
                getTrackpadScrollEvent(
                        MotionEvent.ACTION_DOWN,
                        /* x= */ 13,
                        /* y= */ 24,
                        /* distanceX= */ 15,
                        /* distanceY= */ 99);
        eventForwarder.onTouchEvent(startEvent);

        verifyNativeMouseWheelEventSent(eventForwarder, startEvent, startEvent);

        MotionEvent moveEvent =
                getTrackpadScrollEvent(
                        MotionEvent.ACTION_MOVE,
                        /* x= */ 35,
                        /* y= */ 49,
                        /* distanceX= */ 88,
                        /* distanceY= */ 64);
        eventForwarder.onTouchEvent(moveEvent);

        verifyNativeMouseWheelEventSent(eventForwarder, startEvent, moveEvent);
    }

    private void verifyNativeMouseWheelEventSent(
            EventForwarder eventForwarder,
            MotionEvent trackpadScrollStartEvent,
            MotionEvent trackpadScrollCurrentEvent) {
        verify(mNativeMock, times(1))
                .onMouseWheelEvent(
                        EventForwarderDeviceTest.NATIVE_EVENT_FORWARDER_ID,
                        eventForwarder,
                        MotionEventUtils.getEventTimeNanos(trackpadScrollCurrentEvent),
                        trackpadScrollStartEvent.getX(),
                        trackpadScrollStartEvent.getY(),
                        trackpadScrollStartEvent.getRawX(),
                        trackpadScrollStartEvent.getRawY(),
                        -trackpadScrollCurrentEvent.getAxisValue(
                                MotionEvent.AXIS_GESTURE_SCROLL_X_DISTANCE),
                        -trackpadScrollCurrentEvent.getAxisValue(
                                MotionEvent.AXIS_GESTURE_SCROLL_Y_DISTANCE),
                        trackpadScrollCurrentEvent.getMetaState(),
                        trackpadScrollCurrentEvent.getSource());
    }

    private static MotionEvent getTrackpadScrollEvent(
            int action, float x, float y, float distanceX, float distanceY) {
        return MotionEvent.obtain(
                /* downTime= */ 0,
                /* eventTime= */ 100,
                action,
                /* pointerCount= */ 1,
                getToolTypeFingerProperties(),
                getPointerCoords(x, y, distanceX, distanceY),
                /* metaState= */ 0,
                /* buttonState= */ 0,
                /* xPrecision= */ 0,
                /* yPrecision= */ 0,
                /* deviceId= */ 0,
                /* edgeFlags= */ 0,
                getTrackpadSource(),
                /* displayId= */ 0,
                /* flags= */ 0,
                MotionEvent.CLASSIFICATION_TWO_FINGER_SWIPE);
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

    private static MotionEvent.PointerCoords[] getPointerCoords(
            float x, float y, float distanceX, float distanceY) {
        MotionEvent.PointerCoords[] pointerCoordsArray = new MotionEvent.PointerCoords[1];
        MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
        coords.x = x;
        coords.y = y;
        coords.setAxisValue(MotionEvent.AXIS_GESTURE_SCROLL_X_DISTANCE, distanceX);
        coords.setAxisValue(MotionEvent.AXIS_GESTURE_SCROLL_Y_DISTANCE, distanceY);
        pointerCoordsArray[0] = coords;
        return pointerCoordsArray;
    }

    private static int getTrackpadSource() {
        return InputDevice.SOURCE_MOUSE;
    }
}
