// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Canvas;
import android.os.SystemClock;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.KeyEvent;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.ui.text.ChromeClickableSpan;

/** Unit tests for {@link TextViewWithClickableSpans}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TextViewWithClickableSpansTest {
    @Mock private Canvas mCanvas;
    @Mock private SpanBackgroundHelper mSpanBackgroundHelper;

    private Context mContext;
    private TextViewWithClickableSpans mView;
    private CallbackHelper[] mSpanClickCallbacks;

    @Before
    public void setup() {
        MockitoAnnotations.openMocks(this);
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

    private void addText(int numSpans) {
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
            var span = new ChromeClickableSpan(mContext, (view) -> clickCallback.notifyCalled());
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
