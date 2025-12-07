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
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.ui.util.MotionEventUtils;

/** Tests logic in the {@link EventForwarder} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class EventForwarderTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock EventForwarder.Natives mNativeMock;

    private static final long NATIVE_EVENT_FORWARDER_ID = 1;

    @Before
    public void setUp() {
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
                        dragEvent,
                        /* oldestEventTimeNs= */ eventTime * 1_000_000,
                        /* latestEventTimeNs= */ latestEventTime * 1_000_000,
                        /* action= */ dragEvent.getActionMasked(),
                        /* touchMajor0= */ 0.0f,
                        /* touchMajor1= */ 0.0f,
                        /* touchMinor0= */ 0.0f,
                        /* touchMinor1= */ 0.0f,
                        /* gestureClassification= */ 0,
                        /* isTouchHandleEvent= */ false,
                        /* isLatestEventTimeResampled= */ false);
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
                        dragEvent,
                        /* oldestEventTimeNs= */ latestEventTime * 1_000_000,
                        /* latestEventTimeNs= */ latestEventTime * 1_000_000,
                        /* action= */ dragEvent.getActionMasked(),
                        /* touchMajor0= */ 0.0f,
                        /* touchMajor1= */ 0.0f,
                        /* touchMinor0= */ 0.0f,
                        /* touchMinor1= */ 0.0f,
                        /* gestureClassification= */ 0,
                        /* isTouchHandleEvent= */ false,
                        /* isLatestEventTimeResampled= */ true);
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
                        any(MotionEvent.class),
                        /* oldestEventTimeNs= */ anyLong(),
                        /* latestEventTimeNs= */ anyLong(),
                        /* action= */ anyInt(),
                        /* touchMajor0= */ anyFloat(),
                        /* touchMajor1= */ anyFloat(),
                        /* touchMinor0= */ anyFloat(),
                        /* touchMinor1= */ anyFloat(),
                        /* gestureClassification= */ anyInt(),
                        /* isTouchHandleEvent= */ anyBoolean(),
                        /* isLatestEventTimeResampled= */ anyBoolean());
        verify(mNativeMock, never())
                .onMouseEvent(
                        anyLong(), any(MotionEvent.class), anyLong(), anyInt(), anyInt(), anyInt());
    }

    @Test
    public void testNotSendTrackpadClickAsMouseEventWhenFeatureDisabled() {
        EventForwarder eventForwarder =
                new EventForwarder(NATIVE_EVENT_FORWARDER_ID, true, false, false);
        MotionEvent trackpadClickDownEvent = MotionEventTestUtils.getTrackpadLeftClickEvent();
        eventForwarder.onTouchEvent(trackpadClickDownEvent);
        verify(mNativeMock, never())
                .onMouseEvent(
                        anyLong(), any(MotionEvent.class), anyLong(), anyInt(), anyInt(), anyInt());
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
                        anyLong(), any(MotionEvent.class), anyLong(), anyInt(), anyInt(), anyInt());
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

        MotionEvent.PointerCoords expectedCoords = new MotionEvent.PointerCoords();
        expectedCoords.x = moveEvent.getX() - downEvent.getX();
        expectedCoords.y = moveEvent.getY() - downEvent.getY();

        MotionEvent expectedEvent =
                MotionEvent.obtain(
                        /* downTime= */ moveEvent.getDownTime(),
                        /* eventTime= */ moveEvent.getEventTime(),
                        /* action= */ moveEvent.getAction(),
                        /* pointerCount= */ moveEvent.getPointerCount(),
                        /* pointerProperties= */ MotionEventTestUtils.getToolTypeFingerProperties(
                                moveEvent.getPointerCount()),
                        /* pointerCoords= */ new MotionEvent.PointerCoords[] {expectedCoords},
                        /* metaState= */ moveEvent.getMetaState(),
                        /* buttonState= */ moveEvent.getButtonState(),
                        /* xPrecision= */ 0,
                        /* yPrecision= */ 0,
                        /* deviceId= */ 0,
                        /* edgeFlags= */ 0,
                        /* source= */ InputDevice.SOURCE_MOUSE,
                        /* flags= */ 0);

        ArgumentCaptor<MotionEvent> captor = ArgumentCaptor.forClass(MotionEvent.class);
        verify(mNativeMock, times(1))
                .onMouseEvent(
                        eq(NATIVE_EVENT_FORWARDER_ID),
                        captor.capture(),
                        eq(MotionEventUtils.getEventTimeNanos(expectedEvent)),
                        eq(expectedEvent.getActionMasked()),
                        eq(EventForwarder.getMouseEventActionButton(expectedEvent)),
                        eq(MotionEvent.TOOL_TYPE_MOUSE));
        MotionEventTestUtils.assertEquals(captor.getValue(), expectedEvent);
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
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        /* action= */ MotionEvent.ACTION_BUTTON_PRESS,
                        /* pointerCount= */ pointersCnt,
                        /* pointerProperties= */ MotionEventTestUtils.getToolTypeFingerProperties(
                                pointersCnt),
                        /* pointerCoords= */ MotionEventTestUtils.getPointerCoords(pointersCnt),
                        /* metaState= */ 0,
                        /* buttonState= */ MotionEvent.BUTTON_PRIMARY,
                        /* xPrecision= */ 0,
                        /* yPrecision= */ 0,
                        /* deviceId= */ 0,
                        /* edgeFlags= */ 0,
                        /* source= */ InputDevice.SOURCE_TOUCHPAD,
                        /* flags= */ 0);

        eventForwarder.onCapturedPointerEvent(moveEvent, Surface.ROTATION_0);

        MotionEvent.PointerCoords[] ps = new MotionEvent.PointerCoords[pointersCnt];
        for (int i = 0; i < pointersCnt; i++) {
            ps[i] = new MotionEvent.PointerCoords();
            ps[i].x = i;
            ps[i].y = i;
        }

        MotionEvent transformed =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        /* action= */ MotionEvent.ACTION_BUTTON_PRESS,
                        /* pointerCount= */ pointersCnt,
                        /* pointerProperties= */ MotionEventTestUtils.getToolTypeFingerProperties(
                                pointersCnt),
                        /* pointerCoords= */ ps,
                        /* metaState= */ 0,
                        /* buttonState= */ buttonState,
                        /* xPrecision= */ 0,
                        /* yPrecision= */ 0,
                        /* deviceId= */ 0,
                        /* edgeFlags= */ 0,
                        /* source= */ InputDevice.SOURCE_MOUSE,
                        /* flags= */ 0);

        ArgumentCaptor<MotionEvent> captor = ArgumentCaptor.forClass(MotionEvent.class);
        verify(mNativeMock, times(1))
                .onMouseEvent(
                        eq(NATIVE_EVENT_FORWARDER_ID),
                        captor.capture(),
                        eq(MotionEventUtils.getEventTimeNanos(transformed)),
                        eq(transformed.getActionMasked()),
                        eq(EventForwarder.getMouseEventActionButton(transformed)),
                        eq(MotionEvent.TOOL_TYPE_MOUSE));

        MotionEventTestUtils.assertEquals(captor.getValue(), transformed);
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

        MotionEvent expectedEvent1 =
                MotionEvent.obtain(
                        downTime,
                        eventTime,
                        MotionEvent.ACTION_MOVE,
                        /* x= */ 1,
                        /* y= */ -1,
                        /* metaState= */ 0);
        expectedEvent1.setSource(InputDevice.SOURCE_MOUSE);

        eventForwarder.onCapturedPointerEvent(moveEvent, Surface.ROTATION_0);

        MotionEvent expectedEvent2 =
                MotionEvent.obtain(
                        downTime,
                        eventTime,
                        MotionEvent.ACTION_MOVE,
                        /* x= */ moveEvent.getX() * 2,
                        /* y= */ moveEvent.getY() * 2,
                        /* metaState= */ 0);
        expectedEvent2.setSource(InputDevice.SOURCE_MOUSE);

        eventForwarder.onCapturedPointerEvent(moveEvent, Surface.ROTATION_0);

        ArgumentCaptor<MotionEvent> captor = ArgumentCaptor.forClass(MotionEvent.class);
        verify(mNativeMock, times(2))
                .onMouseEvent(
                        eq(NATIVE_EVENT_FORWARDER_ID),
                        captor.capture(),
                        eq(MotionEventUtils.getEventTimeNanos(moveEvent)),
                        eq(moveEvent.getActionMasked()),
                        eq(EventForwarder.getMouseEventActionButton(moveEvent)),
                        eq(moveEvent.getToolType(0)));
        MotionEventTestUtils.assertEquals(captor.getAllValues().get(0), expectedEvent1);
        MotionEventTestUtils.assertEquals(captor.getAllValues().get(1), expectedEvent2);
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
                .onGenericMotionEvent(anyLong(), any(MotionEvent.class), anyLong(), anyLong());
    }

    private void verifyNativeMouseEventSent(
            long nativeEventForwarder,
            MotionEvent event,
            EventForwarder eventForwarder,
            int times) {
        verify(mNativeMock, times(times))
                .onMouseEvent(
                        nativeEventForwarder,
                        event,
                        MotionEventUtils.getEventTimeNanos(event),
                        event.getActionMasked(),
                        EventForwarder.getMouseEventActionButton(event),
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
