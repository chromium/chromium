// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.ClipData;
import android.content.ClipDescription;
import android.net.Uri;
import android.os.Build;
import android.view.DragEvent;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.MotionEvent.PointerCoords;
import android.view.Surface;
import android.view.View;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.ui.util.MotionEventUtils;

/** Tests logic in the {@link EventForwarder} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class EventForwarderTest {

    @Mock EventForwarder.Natives mNativeMock;

    private static final long NATIVE_EVENT_FORWARDER_ID = 1;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        EventForwarderJni.setInstanceForTesting(mNativeMock);
    }

    @Test
    public void testSendTrackpadClicksAsMouseEventToNative() {
        EventForwarder eventForwarder =
                new EventForwarder(NATIVE_EVENT_FORWARDER_ID, true, true, false);

        // Left click
        MotionEvent leftClickEvent = MotionEventTestUtils.getTrackpadLeftClickEvent();
        eventForwarder.onTouchEvent(leftClickEvent);
        verifyNativeMouseEventSent(NATIVE_EVENT_FORWARDER_ID, leftClickEvent, eventForwarder, 1);

        // Right click
        MotionEvent rightClickEvent = MotionEventTestUtils.getTrackRightClickEvent();
        eventForwarder.onTouchEvent(rightClickEvent);
        verifyNativeMouseEventSent(NATIVE_EVENT_FORWARDER_ID, rightClickEvent, eventForwarder, 1);
    }

    @Test
    public void testSendTrackpadClickReleaseAsMouseEventToNative() {
        EventForwarder eventForwarder =
                new EventForwarder(NATIVE_EVENT_FORWARDER_ID, true, true, false);

        // Left click
        MotionEvent leftClickReleaseEvent =
                MotionEventTestUtils.getTrackpadEvent(
                        MotionEvent.ACTION_BUTTON_RELEASE, MotionEvent.BUTTON_PRIMARY);
        eventForwarder.onTouchEvent(leftClickReleaseEvent);
        verifyNativeMouseEventSent(
                NATIVE_EVENT_FORWARDER_ID, leftClickReleaseEvent, eventForwarder, 1);

        // Right click
        MotionEvent rightClickReleaseEvent =
                MotionEventTestUtils.getTrackpadEvent(
                        MotionEvent.ACTION_BUTTON_RELEASE, MotionEvent.BUTTON_SECONDARY);
        eventForwarder.onTouchEvent(rightClickReleaseEvent);
        verifyNativeMouseEventSent(
                NATIVE_EVENT_FORWARDER_ID, rightClickReleaseEvent, eventForwarder, 1);
    }

    @Test
    public void testSendTrackpadClickAndDragAsMouseEventToNative() {
        EventForwarder eventForwarder =
                new EventForwarder(NATIVE_EVENT_FORWARDER_ID, true, true, false);
        MotionEvent clickAndDragEvent =
                MotionEventTestUtils.getTrackpadEvent(
                        MotionEvent.ACTION_MOVE, MotionEvent.BUTTON_PRIMARY);
        eventForwarder.onTouchEvent(clickAndDragEvent);
        verifyNativeMouseEventSent(NATIVE_EVENT_FORWARDER_ID, clickAndDragEvent, eventForwarder, 1);
    }

    @Test
    public void testSendTrackpadHoverAsMouseEventToNative() {
        EventForwarder eventForwarder =
                new EventForwarder(NATIVE_EVENT_FORWARDER_ID, true, true, false);
        MotionEvent hoverEvent =
                MotionEventTestUtils.getTrackpadEvent(MotionEvent.ACTION_HOVER_MOVE, 0);
        eventForwarder.onHoverEvent(hoverEvent);
        verifyNativeMouseEventSent(NATIVE_EVENT_FORWARDER_ID, hoverEvent, eventForwarder, 1);
    }

    @Test
    public void testMotionEventWithHistory_unbufferedInput() {
        EventForwarder eventForwarder =
                new EventForwarder(NATIVE_EVENT_FORWARDER_ID, true, false, false);
        final long downTime = 100;
        final long eventTime = 200;
        final long latestEventTime = 400;
        MotionEvent dragEvent =
                MotionEvent.obtain(
                        downTime,
                        eventTime,
                        MotionEvent.ACTION_MOVE,
                        /* x= */ 14,
                        /* y= */ 21,
                        /* metaState= */ 0);
        var pointerCoords = new PointerCoords();
        pointerCoords.x = 16;
        pointerCoords.y = 23;
        PointerCoords[] newMovements = {pointerCoords};
        dragEvent.addBatch(latestEventTime, newMovements, /* metaState= */ 0);
        eventForwarder.onTouchEvent(dragEvent);

        // Check that the timestamp is forwarded from the first event in the batch (dragEvent) while
        // coordinates are forwarded from the last (pointerCoords).
        verify(mNativeMock, times(1))
                .onTouchEvent(
                        EventForwarderTest.NATIVE_EVENT_FORWARDER_ID,
                        eventForwarder,
                        dragEvent,
                        eventTime * 1000_000,
                        latestEventTime * 1000_000,
                        downTime,
                        dragEvent.getActionMasked(),
                        1,
                        /* historySize= */ 1,
                        0,
                        pointerCoords.x,
                        pointerCoords.y,
                        0,
                        0,
                        dragEvent.getPointerId(0),
                        -1,
                        0.0f,
                        0.0f,
                        0.0f,
                        0.0f,
                        0.0f,
                        0.0f,
                        dragEvent.getOrientation(0),
                        0,
                        dragEvent.getAxisValue(MotionEvent.AXIS_TILT),
                        0,
                        pointerCoords.x,
                        pointerCoords.y,
                        dragEvent.getToolType(0),
                        MotionEvent.TOOL_TYPE_UNKNOWN,
                        0,
                        dragEvent.getButtonState(),
                        dragEvent.getMetaState(),
                        false,
                        false);
    }

    @Test
    // TODO(https://crbug.com/417198082): Support Android 35+.
    @Config(sdk = Build.VERSION_CODES.UPSIDE_DOWN_CAKE)
    public void testMotionEventWithHistory_bufferedInput() {
        EventForwarder eventForwarder =
                new EventForwarder(NATIVE_EVENT_FORWARDER_ID, true, false, true);
        final long downTime = 100;
        final long eventTime = 200;
        final long latestEventTime = 400;
        MotionEvent dragEvent =
                MotionEvent.obtain(
                        downTime,
                        eventTime,
                        MotionEvent.ACTION_MOVE,
                        /* x= */ 14,
                        /* y= */ 21,
                        /* metaState= */ 0);
        var pointerCoords = new PointerCoords();
        pointerCoords.x = 16;
        pointerCoords.y = 23;
        PointerCoords[] newMovements = {pointerCoords};
        dragEvent.addBatch(latestEventTime, newMovements, /* metaState= */ 0);
        eventForwarder.onTouchEvent(dragEvent);

        // Check that both the timestamp and coordinates are forwarded from the last event in the
        // batch (pointerCoords).
        verify(mNativeMock, times(1))
                .onTouchEvent(
                        EventForwarderTest.NATIVE_EVENT_FORWARDER_ID,
                        eventForwarder,
                        dragEvent,
                        latestEventTime * 1000_000,
                        latestEventTime * 1000_000,
                        downTime,
                        dragEvent.getActionMasked(),
                        1,
                        /* historySize= */ 1,
                        0,
                        pointerCoords.x,
                        pointerCoords.y,
                        0,
                        0,
                        dragEvent.getPointerId(0),
                        -1,
                        0.0f,
                        0.0f,
                        0.0f,
                        0.0f,
                        0.0f,
                        0.0f,
                        dragEvent.getOrientation(0),
                        0,
                        dragEvent.getAxisValue(MotionEvent.AXIS_TILT),
                        0,
                        pointerCoords.x,
                        pointerCoords.y,
                        dragEvent.getToolType(0),
                        MotionEvent.TOOL_TYPE_UNKNOWN,
                        0,
                        dragEvent.getButtonState(),
                        dragEvent.getMetaState(),
                        false,
                        true);
    }

    @Test
    public void testSendTrackEventAsTouchEventWhenButtonIsNotClicked() {
        EventForwarder eventForwarder =
                new EventForwarder(NATIVE_EVENT_FORWARDER_ID, true, true, false);
        MotionEvent trackpadTouchDownEventNoClick =
                MotionEventTestUtils.getTrackpadTouchDownEventNoClick();
        eventForwarder.onTouchEvent(trackpadTouchDownEventNoClick);
        verify(mNativeMock, times(1))
                .onTouchEvent(
                        anyLong(),
                        any(EventForwarder.class),
                        any(MotionEvent.class),
                        anyLong(),
                        anyLong(),
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
                        anyFloat(),
                        anyFloat(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyBoolean(),
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
        EventForwarder eventForwarder =
                new EventForwarder(NATIVE_EVENT_FORWARDER_ID, true, false, false);
        MotionEvent trackpadClickDownEvent = MotionEventTestUtils.getTrackpadLeftClickEvent();
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

    @Test
    public void testDragDropEvent() {
        // Text.
        validateDragDropEvent(
                new String[] {"text/plain"},
                new ClipData.Item[] {new ClipData.Item("text content")},
                new String[][] {}, // expectedFilenames
                "text content", // expectedText
                null, // expectedHtml
                null); // expectedUrl

        // Html.
        validateDragDropEvent(
                new String[] {"text/html"},
                new ClipData.Item[] {new ClipData.Item("text content", "html content")},
                new String[][] {}, // expectedFilenames
                "text content", // expectedText
                "html content", // expectedHtml
                null); // expectedUrl

        // Url.
        validateDragDropEvent(
                new String[] {"text/x-moz-url"},
                new ClipData.Item[] {new ClipData.Item("url content")},
                new String[][] {}, // expectedFilenames
                "url content", // expectedText
                null, // expectedHtml
                "url content"); // expectedUrl

        // Files.
        validateDragDropEvent(
                new String[] {"image/jpeg", "text/plain"},
                new ClipData.Item[] {
                    new ClipData.Item(Uri.parse("image.jpg")),
                    new ClipData.Item(Uri.parse("hello.txt"))
                },
                new String[][] {{"image.jpg", ""}, {"hello.txt", ""}}, // expectedFilenames
                null, // expectedText
                null, // expectedHtml
                null); // expectedUrl
    }

    @Test
    public void testCapturedPointerTrackpadMoveEvent() {
        EventForwarder eventForwarder =
                new EventForwarder(NATIVE_EVENT_FORWARDER_ID, true, true, false);

        MotionEvent moveEvent = MotionEventTestUtils.getCapturedTrackpadMoveEvent(14, 21);

        eventForwarder.onCapturedPointerEvent(moveEvent, Surface.ROTATION_0);
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
    public void testCapturedPointerTrackpadMoveEventAfterDown() {
        EventForwarder eventForwarder =
                new EventForwarder(NATIVE_EVENT_FORWARDER_ID, true, true, false);
        final long downTime = 100;
        final long eventTime = 200;
        MotionEvent downEvent =
                MotionEvent.obtain(
                        downTime,
                        eventTime,
                        MotionEvent.ACTION_DOWN,
                        /* x= */ 14,
                        /* y= */ 21,
                        /* metaState= */ 0);
        downEvent.setSource(InputDevice.SOURCE_TOUCHPAD);
        eventForwarder.onCapturedPointerEvent(downEvent, Surface.ROTATION_0);

        MotionEvent moveEvent = MotionEventTestUtils.getCapturedTrackpadMoveEvent(16, 23);
        eventForwarder.onCapturedPointerEvent(moveEvent, Surface.ROTATION_0);

        verify(mNativeMock, times(1))
                .onMouseEvent(
                        NATIVE_EVENT_FORWARDER_ID,
                        eventForwarder,
                        MotionEventUtils.getEventTimeNanos(moveEvent),
                        moveEvent.getActionMasked(),
                        moveEvent.getX() - downEvent.getX(),
                        moveEvent.getY() - downEvent.getY(),
                        moveEvent.getPointerId(0),
                        moveEvent.getPressure(0),
                        moveEvent.getOrientation(0),
                        moveEvent.getAxisValue(MotionEvent.AXIS_TILT, 0),
                        EventForwarder.getMouseEventActionButton(moveEvent),
                        moveEvent.getButtonState(),
                        moveEvent.getMetaState(),
                        MotionEvent.TOOL_TYPE_MOUSE);
    }

    @Test
    public void testCapturedPointerTrackpadRightClickEvent() {
        testCapturedPointerTrackpadMultiTouchClickEvent(2, MotionEvent.BUTTON_SECONDARY);
    }

    @Test
    public void testCapturedPointerTrackpadMiddleClickEvent() {
        testCapturedPointerTrackpadMultiTouchClickEvent(3, MotionEvent.BUTTON_TERTIARY);
    }

    private void testCapturedPointerTrackpadMultiTouchClickEvent(int pointersCnt, int buttonState) {
        EventForwarder eventForwarder =
                new EventForwarder(NATIVE_EVENT_FORWARDER_ID, true, true, false);

        MotionEvent moveEvent =
                MotionEvent.obtain(
                        0,
                        0,
                        MotionEvent.ACTION_BUTTON_PRESS,
                        pointersCnt,
                        MotionEventTestUtils.getToolTypeFingerProperties(pointersCnt),
                        MotionEventTestUtils.getPointerCoords(pointersCnt),
                        0,
                        MotionEvent.BUTTON_PRIMARY,
                        0,
                        0,
                        0,
                        0,
                        InputDevice.SOURCE_TOUCHPAD,
                        0);

        eventForwarder.onCapturedPointerEvent(moveEvent, Surface.ROTATION_0);
        verify(mNativeMock, times(1))
                .onMouseEvent(
                        NATIVE_EVENT_FORWARDER_ID,
                        eventForwarder,
                        MotionEventUtils.getEventTimeNanos(moveEvent),
                        moveEvent.getActionMasked(),
                        0,
                        0,
                        moveEvent.getPointerId(0),
                        moveEvent.getPressure(0),
                        moveEvent.getOrientation(0),
                        moveEvent.getAxisValue(MotionEvent.AXIS_TILT, 0),
                        EventForwarder.getMouseEventActionButton(moveEvent),
                        buttonState,
                        moveEvent.getMetaState(),
                        MotionEvent.TOOL_TYPE_MOUSE);
    }

    @Test
    public void testCapturedPointerMouseMoveEvent() {
        EventForwarder eventForwarder =
                new EventForwarder(NATIVE_EVENT_FORWARDER_ID, true, true, false);

        final long downTime = 100;
        final long eventTime = 200;
        MotionEvent moveEvent =
                MotionEvent.obtain(
                        downTime,
                        eventTime,
                        MotionEvent.ACTION_MOVE,
                        /* x= */ 1,
                        /* y= */ -1,
                        /* metaState= */ 0);
        moveEvent.setSource(InputDevice.SOURCE_MOUSE_RELATIVE);

        eventForwarder.onCapturedPointerEvent(moveEvent, Surface.ROTATION_0);
        verify(mNativeMock, times(1))
                .onMouseEvent(
                        NATIVE_EVENT_FORWARDER_ID,
                        eventForwarder,
                        MotionEventUtils.getEventTimeNanos(moveEvent),
                        moveEvent.getActionMasked(),
                        moveEvent.getX(),
                        moveEvent.getY(),
                        moveEvent.getPointerId(0),
                        moveEvent.getPressure(0),
                        moveEvent.getOrientation(0),
                        moveEvent.getAxisValue(MotionEvent.AXIS_TILT, 0),
                        EventForwarder.getMouseEventActionButton(moveEvent),
                        moveEvent.getButtonState(),
                        moveEvent.getMetaState(),
                        moveEvent.getToolType(0));

        eventForwarder.onCapturedPointerEvent(moveEvent, Surface.ROTATION_0);
        verify(mNativeMock, times(1))
                .onMouseEvent(
                        NATIVE_EVENT_FORWARDER_ID,
                        eventForwarder,
                        MotionEventUtils.getEventTimeNanos(moveEvent),
                        moveEvent.getActionMasked(),
                        moveEvent.getX() * 2,
                        moveEvent.getY() * 2,
                        moveEvent.getPointerId(0),
                        moveEvent.getPressure(0),
                        moveEvent.getOrientation(0),
                        moveEvent.getAxisValue(MotionEvent.AXIS_TILT, 0),
                        EventForwarder.getMouseEventActionButton(moveEvent),
                        moveEvent.getButtonState(),
                        moveEvent.getMetaState(),
                        moveEvent.getToolType(0));
    }

    @Test
    public void testCapturedPointerMouseScrollEvent() {
        EventForwarder eventForwarder =
                new EventForwarder(NATIVE_EVENT_FORWARDER_ID, true, true, false);

        final long downTime = 100;
        final long eventTime = 200;
        MotionEvent scrollEvent =
                MotionEvent.obtain(
                        downTime,
                        eventTime,
                        MotionEvent.ACTION_SCROLL,
                        /* x= */ 0,
                        /* y= */ 0,
                        /* metaState= */ 0);
        scrollEvent.setSource(InputDevice.SOURCE_MOUSE_RELATIVE);

        eventForwarder.onCapturedPointerEvent(scrollEvent, Surface.ROTATION_0);
        verify(mNativeMock, times(1))
                .onGenericMotionEvent(
                        anyLong(),
                        any(EventForwarder.class),
                        any(MotionEvent.class),
                        anyLong(),
                        anyLong());
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

    private void validateDragDropEvent(
            String[] mimeTypes,
            ClipData.Item[] items,
            String[][] expectedFilenames,
            String expectedText,
            String expectedHtml,
            String expectedUrl) {
        ClipData clipData = new ClipData("label", mimeTypes, items[0]);
        for (int i = 1; i < items.length; i++) {
            clipData.addItem(items[i]);
        }
        ClipDescription clipDescription = new ClipDescription("label", mimeTypes);
        EventForwarder eventForwarder =
                new EventForwarder(NATIVE_EVENT_FORWARDER_ID, true, false, false);
        DragEvent event = mock(DragEvent.class);
        doReturn(DragEvent.ACTION_DROP).when(event).getAction();
        doReturn(14f).when(event).getX();
        doReturn(21f).when(event).getY();
        doReturn(clipData).when(event).getClipData();
        doReturn(clipDescription).when(event).getClipDescription();
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectIntRecord("Android.DragDrop.Files.Count", expectedFilenames.length)
                        .build();
        eventForwarder.onDragEvent(event, mock(View.class));
        verify(mNativeMock, times(1))
                .onDragEvent(
                        eq(EventForwarderTest.NATIVE_EVENT_FORWARDER_ID),
                        eq(eventForwarder),
                        eq(DragEvent.ACTION_DROP),
                        eq(14.0f), // x
                        eq(21.0f), // y
                        eq(14.0f), // screenX
                        eq(21.0f), // screenY
                        eq(mimeTypes),
                        eq(""), // content
                        argThat(
                                filenames -> {
                                    if (filenames.length != expectedFilenames.length) {
                                        return false;
                                    }
                                    for (int i = 0; i < filenames.length; i++) {
                                        if (filenames[i].length != 2
                                                || !expectedFilenames[i][0].equals(filenames[i][0])
                                                || !expectedFilenames[i][1].equals(
                                                        filenames[i][1])) {
                                            return false;
                                        }
                                    }
                                    return true;
                                }),
                        eq(expectedText),
                        eq(expectedHtml),
                        eq(expectedUrl));
        histograms.assertExpected();
    }
}
