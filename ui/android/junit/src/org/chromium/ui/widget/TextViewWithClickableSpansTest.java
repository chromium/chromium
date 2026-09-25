// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Canvas;
import android.os.SystemClock;
import android.text.Layout;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View.MeasureSpec;
import android.view.accessibility.AccessibilityNodeInfo;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.ui.text.ChromeClickableSpan;

/** Unit tests for {@link TextViewWithClickableSpans}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TextViewWithClickableSpansTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Canvas mCanvas;
    @Mock private SpanBackgroundHelper mSpanBackgroundHelper;

    private Context mContext;
    private TextViewWithClickableSpans mView;
    private CallbackHelper[] mSpanClickCallbacks;

    @Before
    public void setup() {
        mContext = ApplicationProvider.getApplicationContext();
        mView = new TextViewWithClickableSpans(mContext, /* attrs= */ null, mSpanBackgroundHelper);
    }

    @Test
    public void testTextWithClickableSpan_Focused() {
        addText(/* numSpans= */ 1);
        int clickCallbackCount = mSpanClickCallbacks[0].getCallCount();
        mView.getOnFocusChangeListener().onFocusChange(mView, true);

        ClickableSpan[] clickableSpans = mView.getClickableSpans();
        int focusedIndex = mView.getFocusedSpanIndexForTesting();
        assertEquals("Focused span index is incorrect", 0, focusedIndex);
        assertTrue(
                "ChromeClickableSpan should be focused.",
                ((ChromeClickableSpan) clickableSpans[focusedIndex]).isFocused());
        assertTrue(
                "Enter key press should be handled when span is focused.", simulateEnterKeyPress());
        assertEquals(
                "Span click callback should run on enter key press.",
                clickCallbackCount + 1,
                mSpanClickCallbacks[0].getCallCount());
    }

    @Test
    public void testTextWithClickableSpan_Unfocused() {
        addText(/* numSpans= */ 1);
        // Focus view first.
        mView.getOnFocusChangeListener().onFocusChange(mView, true);
        int focusedIndex = mView.getFocusedSpanIndexForTesting();

        // Clear view focus.
        mView.getOnFocusChangeListener().onFocusChange(mView, false);

        ClickableSpan[] clickableSpans = mView.getClickableSpans();
        assertEquals(
                "Focused span index should be reset.", -1, mView.getFocusedSpanIndexForTesting());
        assertFalse(
                "ChromeClickableSpan should be unfocused.",
                ((ChromeClickableSpan) clickableSpans[focusedIndex]).isFocused());
        assertFalse(
                "Enter key press should not be handled when view is not focused.",
                simulateEnterKeyPress());
    }

    @Test
    public void testTextWithoutClickableSpan_Focused() {
        addText(/* numSpans= */ 0);
        mView.getOnFocusChangeListener().onFocusChange(mView, true);
        assertEquals("Focused span index is incorrect.", -1, mView.getFocusedSpanIndexForTesting());
        assertFalse(
                "Enter key press should not be handled when view is not focused.",
                simulateEnterKeyPress());
    }

    @Test
    public void testOnDrawWithFocusedSpan() {
        addText(/* numSpans= */ 1);

        mView.getOnFocusChangeListener().onFocusChange(mView, true);
        // Trigger #onDraw() that is invoked after #invalidate() on a focus change.
        mView.onDraw(mCanvas);

        verify(mSpanBackgroundHelper)
                .drawFocusedSpanBackground(
                        mCanvas,
                        (Spanned) mView.getText(),
                        mView.getClickableSpans()[0],
                        mView.getLayout());
    }

    @Test
    public void testTextWithMultipleClickableSpans_FocusForward() {
        // Create view with text containing two spans.
        addText(/* numSpans= */ 2);
        var clickableSpans = mView.getClickableSpans();
        int clickCallbackCount0 = mSpanClickCallbacks[0].getCallCount();
        int clickCallbackCount1 = mSpanClickCallbacks[1].getCallCount();

        // Focus view, so that span0 is focused.
        mView.getOnFocusChangeListener().onFocusChange(mView, true);

        // Focus forward to navigate to span1.
        simulateTabKeyPress(/* isShiftPressed= */ false);
        assertEquals("Focused span index is incorrect.", 1, mView.getFocusedSpanIndexForTesting());

        assertFalse(
                "Previous span's focus should be cleared.",
                ((ChromeClickableSpan) clickableSpans[0]).isFocused());
        assertTrue(
                "New span should be focused.",
                ((ChromeClickableSpan) clickableSpans[1]).isFocused());

        // Dispatch enter key press.
        assertTrue(
                "Enter key press should be handled when span is focused.", simulateEnterKeyPress());
        assertEquals(
                "New span's click callback should run on enter key press.",
                clickCallbackCount1 + 1,
                mSpanClickCallbacks[1].getCallCount());
        assertEquals(
                "Old span's click callback should not run on enter key press.",
                clickCallbackCount0,
                mSpanClickCallbacks[0].getCallCount());

        // Focus forward, no other span is present.
        simulateTabKeyPress(/* isShiftPressed= */ false);
        assertEquals(
                "Focused span index should be reset.", -1, mView.getFocusedSpanIndexForTesting());
    }

    @Test
    public void testTextWithMultipleClickableSpans_FocusBackward() {
        // Create view with text containing two spans.
        addText(/* numSpans= */ 2);
        var clickableSpans = mView.getClickableSpans();
        int clickCallbackCount0 = mSpanClickCallbacks[0].getCallCount();
        int clickCallbackCount1 = mSpanClickCallbacks[1].getCallCount();

        // Focus view, and focus forward to navigate to span1.
        mView.getOnFocusChangeListener().onFocusChange(mView, true);
        simulateTabKeyPress(/* isShiftPressed= */ false);

        // Focus backward to navigate to span0.
        simulateTabKeyPress(/* isShiftPressed= */ true);
        assertEquals("Focused span index is incorrect.", 0, mView.getFocusedSpanIndexForTesting());

        assertFalse(
                "Previous span's focus should be cleared.",
                ((ChromeClickableSpan) clickableSpans[1]).isFocused());
        assertTrue(
                "New span should be focused.",
                ((ChromeClickableSpan) clickableSpans[0]).isFocused());

        // Dispatch enter key press.
        assertTrue(
                "Enter key press should be handled when span is focused.", simulateEnterKeyPress());
        assertEquals(
                "New span's click callback should run on enter key press.",
                clickCallbackCount0 + 1,
                mSpanClickCallbacks[0].getCallCount());
        assertEquals(
                "Old span's click callback should not run on enter key press.",
                clickCallbackCount1,
                mSpanClickCallbacks[1].getCallCount());

        // Focus backward, no other span is present.
        simulateTabKeyPress(/* isShiftPressed= */ true);
        assertEquals(
                "Focused span index should be reset.", -1, mView.getFocusedSpanIndexForTesting());
    }

    @Test
    public void testOnInitializeAccessibilityNodeInfo_NoSpans() {
        addText(0);
        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();
        mView.onInitializeAccessibilityNodeInfo(info);
        assertNull(info.getContentDescription());
    }

    @Test
    public void testOnInitializeAccessibilityNodeInfo_ClickableSpanWithoutDescription() {
        addText(1);
        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();
        mView.onInitializeAccessibilityNodeInfo(info);
        assertNull(info.getContentDescription());
    }

    @Test
    public void testOnInitializeAccessibilityNodeInfo_ClickableSpanWithDescription() {
        addText(2, "first link contentDescription.");
        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();
        mView.onInitializeAccessibilityNodeInfo(info);
        assertEquals(
                "This text is not clickable. first link contentDescription. This is clickable"
                        + " text1.",
                info.getContentDescription().toString());
    }

    @Test
    public void testOnInitializeAccessibilityNodeInfo_ViewAlreadyHasContentDescription() {
        mView.setContentDescription("Explicit view description");
        addText(2, "first link contentDescription.");
        AccessibilityNodeInfo info = AccessibilityNodeInfo.obtain();
        mView.onInitializeAccessibilityNodeInfo(info);
        // When content description is set, info is unmodified.
        assertNull(info.getContentDescription());
    }

    @Test
    public void testOnGenericMotionEvent_SecondaryClickOnSpan() {
        TextViewWithClickableSpans view =
                new TextViewWithClickableSpans(mContext, /* attrs= */ null, mSpanBackgroundHelper) {
                    @Override
                    protected boolean touchIntersectsAnyClickableSpans(MotionEvent event) {
                        return event.getX() < 50f;
                    }
                };
        CallbackHelper longClickCallback = new CallbackHelper();
        view.setOnSpanLongClickListener(
                _ -> {
                    longClickCallback.notifyCalled();
                    return true;
                });

        MotionEvent secondaryClickOnSpan =
                createMouseButtonPressEvent(
                        /* x= */ 10f, /* y= */ 10f, MotionEvent.BUTTON_SECONDARY);
        assertTrue(view.onGenericMotionEvent(secondaryClickOnSpan));
        assertEquals(1, longClickCallback.getCallCount());
        secondaryClickOnSpan.recycle();

        MotionEvent secondaryClickOutsideSpan =
                createMouseButtonPressEvent(
                        /* x= */ 100f, /* y= */ 10f, MotionEvent.BUTTON_SECONDARY);
        assertFalse(view.onGenericMotionEvent(secondaryClickOutsideSpan));
        assertEquals(1, longClickCallback.getCallCount());
        secondaryClickOutsideSpan.recycle();
    }

    @Test
    public void testOnLongClick_IntersectsClickableSpan() {
        TextViewWithClickableSpans view =
                new TextViewWithClickableSpans(mContext, /* attrs= */ null, mSpanBackgroundHelper) {
                    @Override
                    protected boolean touchIntersectsAnyClickableSpans(MotionEvent event) {
                        return event.getX() < 50f;
                    }
                };
        CallbackHelper longClickCallback = new CallbackHelper();
        view.setOnSpanLongClickListener(
                _ -> {
                    longClickCallback.notifyCalled();
                    return true;
                });

        MotionEvent downOnSpan =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        MotionEvent.ACTION_DOWN,
                        /* x= */ 10f,
                        /* y= */ 10f,
                        /* metaState= */ 0);
        view.onTouchEvent(downOnSpan);
        assertTrue(view.onLongClick(view));
        assertEquals(1, longClickCallback.getCallCount());
        downOnSpan.recycle();

        MotionEvent upOnSpan =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        MotionEvent.ACTION_UP,
                        /* x= */ 10f,
                        /* y= */ 10f,
                        /* metaState= */ 0);
        view.onTouchEvent(upOnSpan);
        assertFalse(
                "Long click should not trigger after ACTION_UP clears touch state.",
                view.onLongClick(view));
        assertEquals(1, longClickCallback.getCallCount());
        upOnSpan.recycle();

        MotionEvent downBeforeCancel =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        MotionEvent.ACTION_DOWN,
                        /* x= */ 10f,
                        /* y= */ 10f,
                        /* metaState= */ 0);
        view.onTouchEvent(downBeforeCancel);
        downBeforeCancel.recycle();

        MotionEvent cancelEvent =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        MotionEvent.ACTION_CANCEL,
                        /* x= */ 10f,
                        /* y= */ 10f,
                        /* metaState= */ 0);
        view.onTouchEvent(cancelEvent);
        assertFalse(
                "Long click should not trigger after ACTION_CANCEL clears touch state.",
                view.onLongClick(view));
        assertEquals(1, longClickCallback.getCallCount());
        cancelEvent.recycle();

        MotionEvent downOutsideSpan =
                MotionEvent.obtain(
                        /* downTime= */ 0,
                        /* eventTime= */ 0,
                        MotionEvent.ACTION_DOWN,
                        /* x= */ 100f,
                        /* y= */ 10f,
                        /* metaState= */ 0);
        view.onTouchEvent(downOutsideSpan);
        assertFalse(view.onLongClick(view));
        assertEquals(1, longClickCallback.getCallCount());
        downOutsideSpan.recycle();
    }

    @Test
    public void testOnLongClick_FocusedSpan() {
        addText(/* numSpans= */ 1);
        CallbackHelper longClickCallback = new CallbackHelper();
        mView.setOnSpanLongClickListener(
                _ -> {
                    longClickCallback.notifyCalled();
                    return true;
                });

        assertFalse(
                "Long click should not trigger when no span is focused or touched.",
                mView.onLongClick(mView));
        assertEquals(0, longClickCallback.getCallCount());

        mView.getOnFocusChangeListener().onFocusChange(mView, true);
        assertTrue(
                "Long click should trigger when a clickable span is keyboard-focused.",
                mView.onLongClick(mView));
        assertEquals(1, longClickCallback.getCallCount());
    }

    @Test
    public void testTouchIntersectsWrappedClickableSpan() {
        String prefix = "Unclickable prefix text ";
        String firstLineSpan = "clickable span first line\n";
        String secondLineSpan = "clickable span second line";
        SpannableStringBuilder text = new SpannableStringBuilder(prefix);
        int spanStart = text.length();
        text.append(firstLineSpan).append(secondLineSpan);
        int spanEnd = text.length();
        ChromeClickableSpan span =
                new ChromeClickableSpan(mContext, _ -> {}, /* contentDescription= */ null);
        text.setSpan(span, spanStart, spanEnd, /* flags= */ 0);
        mView.setText(text);

        mView.measure(
                MeasureSpec.makeMeasureSpec(1000, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(500, MeasureSpec.EXACTLY));
        mView.layout(/* l= */ 0, /* t= */ 0, /* r= */ 1000, /* b= */ 500);

        Layout layout = mView.getLayout();
        assertEquals(2, layout.getLineCount());

        float line0Y = (layout.getLineTop(0) + layout.getLineBottom(0)) / 2f;
        float prefixX = layout.getPrimaryHorizontal(spanStart) / 2f;
        float line0SpanX = (layout.getPrimaryHorizontal(spanStart) + layout.getLineRight(0)) / 2f;

        MotionEvent clickOnPrefix =
                createMouseButtonPressEvent(prefixX, line0Y, MotionEvent.BUTTON_SECONDARY);
        assertFalse(
                "Touch on prefix before wrapped span on line 0 should not intersect span.",
                mView.touchIntersectsAnyClickableSpans(clickOnPrefix));
        clickOnPrefix.recycle();

        MotionEvent clickOnFirstLineSpan =
                createMouseButtonPressEvent(line0SpanX, line0Y, MotionEvent.BUTTON_SECONDARY);
        assertTrue(
                "Touch on first line of wrapped span should intersect span.",
                mView.touchIntersectsAnyClickableSpans(clickOnFirstLineSpan));
        clickOnFirstLineSpan.recycle();

        float line1Y = (layout.getLineTop(1) + layout.getLineBottom(1)) / 2f;
        float line1SpanX = (layout.getLineLeft(1) + layout.getPrimaryHorizontal(spanEnd)) / 2f;
        MotionEvent clickOnSecondLineSpan =
                createMouseButtonPressEvent(line1SpanX, line1Y, MotionEvent.BUTTON_SECONDARY);
        assertTrue(
                "Touch on second line of wrapped span should intersect span.",
                mView.touchIntersectsAnyClickableSpans(clickOnSecondLineSpan));
        clickOnSecondLineSpan.recycle();
    }

    private static MotionEvent createMouseButtonPressEvent(float x, float y, int buttonState) {
        MotionEvent.PointerProperties props = new MotionEvent.PointerProperties();
        props.id = 0;
        props.toolType = MotionEvent.TOOL_TYPE_MOUSE;

        MotionEvent.PointerCoords coords = new MotionEvent.PointerCoords();
        coords.x = x;
        coords.y = y;

        return MotionEvent.obtain(
                /* downTime= */ 0,
                /* eventTime= */ 0,
                MotionEvent.ACTION_BUTTON_PRESS,
                /* pointerCount= */ 1,
                new MotionEvent.PointerProperties[] {props},
                new MotionEvent.PointerCoords[] {coords},
                /* metaState= */ 0,
                buttonState,
                /* xPrecision= */ 1f,
                /* yPrecision= */ 1f,
                /* deviceId= */ 0,
                /* edgeFlags= */ 0,
                InputDevice.SOURCE_MOUSE,
                /* flags= */ 0);
    }

    private void addText(int numSpans, String... contentDescriptions) {
        String plainText = "This text is not clickable.";
        if (numSpans == 0) {
            mView.setText(plainText);
            return;
        }

        var text = new SpannableStringBuilder(plainText);
        mSpanClickCallbacks = new CallbackHelper[numSpans];
        for (int i = 0; i < numSpans; i++) {
            int spanStart = text.length() + 1;
            String clickableText = " This is clickable text" + i + ".";
            text.append(clickableText);
            var clickCallback = new CallbackHelper();
            mSpanClickCallbacks[i] = clickCallback;
            var contentDesc = i < contentDescriptions.length ? contentDescriptions[i] : null;
            var span =
                    new ChromeClickableSpan(
                            mContext, (view) -> clickCallback.notifyCalled(), contentDesc);
            text.setSpan(span, spanStart, text.length(), /* flags= */ 0);
        }
        mView.setText(text);
    }

    private boolean simulateEnterKeyPress() {
        return mView.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));
    }

    private void simulateTabKeyPress(boolean isShiftPressed) {
        if (!isShiftPressed) {
            mView.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_DOWN, KeyEvent.KEYCODE_TAB));
        } else {
            long eventTime = SystemClock.uptimeMillis();
            mView.dispatchKeyEvent(
                    new KeyEvent(
                            eventTime,
                            eventTime,
                            KeyEvent.ACTION_DOWN,
                            KeyEvent.KEYCODE_TAB,
                            0,
                            KeyEvent.META_SHIFT_ON));
        }
    }
}
