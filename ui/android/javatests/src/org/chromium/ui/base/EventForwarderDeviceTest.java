// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.os.Build;
import android.os.SystemClock;
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
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.ui.util.MotionEventUtils;

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

    @Mock EventForwarder.Natives mNativeMock;

    private static final long NATIVE_EVENT_FORWARDER_ID = 1;

    @Before
    public void setUp() {
        EventForwarderJni.setInstanceForTesting(mNativeMock);
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testSendTrackpadScrollAsMouseWheelToNativeAtLeastU() {
        EventForwarder eventForwarder =
                new EventForwarder(NATIVE_EVENT_FORWARDER_ID, true, true, false);

        final long downTime = SystemClock.uptimeMillis();
        long eventTime = downTime;
        MotionEvent startEvent =
                getTrackpadScrollEvent(
                        MotionEvent.ACTION_DOWN, /* x= */ 13, /* y= */ 24, downTime, eventTime);
        eventForwarder.onTouchEvent(startEvent);

        verifyNativeMouseWheelEventSent(startEvent, startEvent);

        // Move for a small distance from last event so velocity would not trigger fling.
        eventTime += 50;
        MotionEvent moveEvent =
                getTrackpadScrollEvent(
                        MotionEvent.ACTION_MOVE, /* x= */ 13, /* y= */ 26, downTime, eventTime);
        eventForwarder.onTouchEvent(moveEvent);

        verifyNativeMouseWheelEventSent(startEvent, moveEvent);

        eventTime += 50;
        MotionEvent upEvent =
                getTrackpadScrollEvent(
                        MotionEvent.ACTION_UP, /* x= */ 13, /* y= */ 26, downTime, eventTime);
        eventForwarder.onTouchEvent(upEvent);

        verifyNativeMouseWheelEventSent(moveEvent, upEvent);

        verifyNativeStartFlingEventNotSent();
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testSendTrackpadStartFlingToNativeAtLeastU() {
        EventForwarder eventForwarder =
                new EventForwarder(NATIVE_EVENT_FORWARDER_ID, true, true, false);

        final long downTime = SystemClock.uptimeMillis();
        long eventTime = downTime;
        MotionEvent startEvent =
                getTrackpadScrollEvent(
                        MotionEvent.ACTION_DOWN, /* x= */ 13, /* y= */ 24, downTime, eventTime);
        eventForwarder.onTouchEvent(startEvent);

        verifyNativeMouseWheelEventSent(startEvent, startEvent);

        // Move for sufficiently large distance from last event so velocity should trigger fling.
        eventTime += 20;
        MotionEvent moveEvent =
                getTrackpadScrollEvent(
                        MotionEvent.ACTION_MOVE, /* x= */ 15, /* y= */ 854, downTime, eventTime);
        eventForwarder.onTouchEvent(moveEvent);

        verifyNativeMouseWheelEventSent(startEvent, moveEvent);

        eventTime += 20;
        MotionEvent upEvent =
                getTrackpadScrollEvent(
                        MotionEvent.ACTION_UP, /* x= */ 18, /* y= */ 854, downTime, eventTime);
        eventForwarder.onTouchEvent(upEvent);

        verifyNativeStartFlingEventSent(upEvent);
    }

    private void verifyNativeMouseWheelEventSent(
            MotionEvent trackpadScrollLastEvent, MotionEvent trackpadScrollCurrentEvent) {
        verify(mNativeMock, times(1))
                .onMouseWheelEvent(
                        eq(EventForwarderDeviceTest.NATIVE_EVENT_FORWARDER_ID),
                        eq(trackpadScrollCurrentEvent),
                        eq(MotionEventUtils.getEventTimeNanos(trackpadScrollCurrentEvent)),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        eq(trackpadScrollCurrentEvent.getX() - trackpadScrollLastEvent.getX()),
                        eq(trackpadScrollCurrentEvent.getY() - trackpadScrollLastEvent.getY()));
    }

    private void verifyNativeStartFlingEventNotSent() {
        verify(mNativeMock, times(0))
                .startFling(
                        anyLong(),
                        anyLong(),
                        anyFloat(),
                        anyFloat(),
                        anyBoolean(),
                        anyBoolean(),
                        anyBoolean());
    }

    private void verifyNativeStartFlingEventSent(MotionEvent trackpadScrollCurrentEvent) {
        verify(mNativeMock, times(1))
                .startFling(
                        eq(EventForwarderDeviceTest.NATIVE_EVENT_FORWARDER_ID),
                        eq(trackpadScrollCurrentEvent.getEventTime()),
                        anyFloat(),
                        anyFloat(),
                        eq(false),
                        eq(false),
                        eq(true));
    }

    private static MotionEvent getTrackpadScrollEvent(
            int action, float x, float y, long downTime, long eventTime) {
        return MotionEvent.obtain(
                /* downTime= */ downTime,
                /* eventTime= */ eventTime,
                action,
                /* pointerCount= */ 1,
                getToolTypeFingerProperties(),
                getPointerCoords(x, y),
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

    private static MotionEvent.PointerCoords[] getPointerCoords(float x, float y) {
        MotionEvent.PointerCoords[] pointerCoordsArray = new MotionEvent.PointerCoords[1];
        MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
        coords.x = x;
        coords.y = y;
        pointerCoordsArray[0] = coords;
        return pointerCoordsArray;
    }

    private static int getTrackpadSource() {
        return InputDevice.SOURCE_MOUSE;
    }
}
