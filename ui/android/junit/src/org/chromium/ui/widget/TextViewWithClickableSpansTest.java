// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Canvas;
import android.text.SpannableString;
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
    private SpannableString mText;
    private ChromeClickableSpan mSpan;
    private final CallbackHelper mSpanClickCallback = new CallbackHelper();

    @Before
    public void setup() {
        MockitoAnnotations.openMocks(this);
        mContext = ApplicationProvider.getApplicationContext();
        mView = new TextViewWithClickableSpans(mContext, /* attrs= */ null, mSpanBackgroundHelper);
    }

    @Test
    public void testTextWithClickableSpan_Focused() {
        addText(/* containsClickableSpan= */ true);
        int clickCallbackCount = mSpanClickCallback.getCallCount();
        mView.getOnFocusChangeListener().onFocusChange(mView, true);
        assertNotNull("Focused span should be non-null.", mView.getFocusedSpanForTesting());
        assertTrue("ChromeClickableSpan should be focused.", mSpan.isFocused());
        assertTrue(
                "Enter key press should be handled when span is focused.", simulateEnterKeyPress());
        assertEquals(
                "Span click callback should run on enter key press.",
                clickCallbackCount + 1,
                mSpanClickCallback.getCallCount());
    }

    @Test
    public void testTextWithClickableSpan_Unfocused() {
        addText(/* containsClickableSpan= */ true);
        int clickCallbackCount = mSpanClickCallback.getCallCount();
        // Focus view first.
        mView.getOnFocusChangeListener().onFocusChange(mView, true);

        mView.getOnFocusChangeListener().onFocusChange(mView, false);
        assertNull("Focused span should be null.", mView.getFocusedSpanForTesting());
        assertFalse("ChromeClickableSpan should be unfocused.", mSpan.isFocused());
        simulateEnterKeyPress();
        assertEquals(
                "Span click callback should not run on enter key press.",
                clickCallbackCount,
                mSpanClickCallback.getCallCount());
    }

    @Test
    public void testTextWithoutClickableSpan_Focused() {
        addText(/* containsClickableSpan= */ false);
        mView.getOnFocusChangeListener().onFocusChange(mView, true);
        assertNull("Focused span should be null.", mView.getFocusedSpanForTesting());
    }

    @Test
    public void testOnDrawWithFocusedSpan() {
        addText(/* containsClickableSpan= */ true);
        mView.getOnFocusChangeListener().onFocusChange(mView, true);
        assertNotNull("Focused span should not be null.", mView.getFocusedSpanForTesting());

        mView.onDraw(mCanvas);
        verify(mSpanBackgroundHelper)
                .drawFocusedSpanBackground(
                        eq(mCanvas), eq(mText), eq(mSpan), eq(mView.getLayout()));
    }

    private void addText(boolean containsClickableSpan) {
        String plainText = "This text is not clickable.";
        String clickableText = "This text is clickable.";
        mText =
                new SpannableString(
                        containsClickableSpan ? plainText + " " + clickableText : plainText);
        if (containsClickableSpan) {
            mSpan = new ChromeClickableSpan(mContext, (view) -> mSpanClickCallback.notifyCalled());
            mText.setSpan(mSpan, plainText.length() + 1, mText.length(), /* flags= */ 0);
        }
        mView.setText(mText);
    }

    private boolean simulateEnterKeyPress() {
        return mView.dispatchKeyEvent(new KeyEvent(KeyEvent.ACTION_UP, KeyEvent.KEYCODE_ENTER));
    }
}
