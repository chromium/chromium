// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.text.Layout;
import android.text.Spanned;
import android.text.style.ClickableSpan;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.Spy;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.R;

/** Unit tests for {@link SpanBackgroundHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SpanBackgroundHelperTest {
    private static class TestTextSpanSpec {
        private final int mSpanStart;
        private final int mSpanEnd;
        private final int mStartLine;
        private final int mEndLine;
        private final float mPrimaryHorizontalStart;
        private final float mPrimaryHorizontalEnd;
        private final boolean mRtl;

        private TestTextSpanSpec(
                int spanStart,
                int spanEnd,
                int startLine,
                int endLine,
                float primaryHorizontalStart,
                float primaryHorizontalEnd,
                boolean rtl) {
            mSpanStart = spanStart;
            mSpanEnd = spanEnd;
            mStartLine = startLine;
            mEndLine = endLine;
            mPrimaryHorizontalStart = primaryHorizontalStart;
            mPrimaryHorizontalEnd = primaryHorizontalEnd;
            mRtl = rtl;
        }
    }

    // The test horizontal padding of the span.
    private static final int HORIZONTAL_PADDING = 2;
    // The test position of the leftmost char on any line.
    private static final float LINE_LEFT = 0;
    // The test position of the rightmost char on any line.
    private static final float LINE_RIGHT = 60;
    // The test height of text on a single line.
    private static final int TEXT_HEIGHT = 10;

    @Mock private Canvas mCanvas;
    @Mock private Layout mLayout;
    @Mock private Spanned mText;
    @Mock private ClickableSpan mClickableSpan;

    @Spy private Drawable mDrawable;

    private SpanBackgroundHelper mSpanBackgroundHelper;

    @Before
    public void setup() {
        MockitoAnnotations.openMocks(this);
        Context mContext = ApplicationProvider.getApplicationContext();
        mDrawable = spy(AppCompatResources.getDrawable(mContext, R.drawable.span_background));
        mSpanBackgroundHelper = new SpanBackgroundHelper(HORIZONTAL_PADDING, mDrawable);
    }

    @Test
    public void testSingleLineSpan_Ltr() {
        final TestTextSpanSpec spec =
                new TestTextSpanSpec(
                        /* spanStart= */ 2,
                        /* spanEnd= */ 4,
                        /* startLine= */ 2,
                        /* endLine= */ 2,
                        /* primaryHorizontalStart= */ 20,
                        /* primaryHorizontalEnd= */ 40,
                        /* rtl= */ false);
        setupText(spec);
        mSpanBackgroundHelper.drawFocusedSpanBackground(mCanvas, mText, mClickableSpan, mLayout);

        // Verify line background bounds:
        // left = primaryHorizontalStart - HORIZONTAL_PADDING = 20 - 2 = 18.
        // top = startLine * TEXT_HEIGHT = 2 * 10 = 20.
        // right = primaryHorizontalEnd + HORIZONTAL_PADDING = 40 + 2 = 42.
        // bottom = (startLine + 1) * TEXT_HEIGHT = 3 * 10 = 30.
        Rect expected = new Rect(18, 20, 42, 30);
        assertEquals("Background bounds are incorrect.", expected, mDrawable.getBounds());
        verify(mDrawable).draw(mCanvas);
    }

    @Test
    public void testSingleLineSpan_Rtl() {
        final TestTextSpanSpec spec =
                new TestTextSpanSpec(
                        /* spanStart= */ 2,
                        /* spanEnd= */ 4,
                        /* startLine= */ 2,
                        /* endLine= */ 2,
                        /* primaryHorizontalStart= */ 40,
                        /* primaryHorizontalEnd= */ 20,
                        /* rtl= */ true);
        setupText(spec);
        mSpanBackgroundHelper.drawFocusedSpanBackground(mCanvas, mText, mClickableSpan, mLayout);

        // Verify line background bounds:
        // left = primaryHorizontalEnd - HORIZONTAL_PADDING = 20 - 2 = 18.
        // top = startLine * TEXT_HEIGHT = 2 * 10 = 20.
        // right = primaryHorizontalStart + HORIZONTAL_PADDING = 40 + 2 = 42.
        // bottom = (startLine + 1) * TEXT_HEIGHT = 3 * 10 = 30.
        Rect expected = new Rect(18, 20, 42, 30);
        assertEquals("Background bounds are incorrect.", expected, mDrawable.getBounds());
        verify(mDrawable).draw(mCanvas);
    }

    @Test
    public void testMultiLineSpan_Ltr() {
        final TestTextSpanSpec spec =
                new TestTextSpanSpec(
                        /* spanStart= */ 5,
                        /* spanEnd= */ 15,
                        /* startLine= */ 0,
                        /* endLine= */ 2,
                        /* primaryHorizontalStart= */ 50,
                        /* primaryHorizontalEnd= */ 10,
                        /* rtl= */ false);
        setupText(spec);
        mSpanBackgroundHelper.drawFocusedSpanBackground(mCanvas, mText, mClickableSpan, mLayout);

        // Verify first line background bounds:
        // left = primaryHorizontalStart - HORIZONTAL_PADDING = 50 - 2 = 48.
        // top = startLine * TEXT_HEIGHT = 0 * 10 = 0.
        // right = LINE_RIGHT + HORIZONTAL_PADDING = 60 + 2 = 62.
        // bottom = (startLine + 1) * TEXT_HEIGHT = 1 * 10 = 10.
        verify(mDrawable, times(1)).setBounds(eq(48), eq(0), eq(62), eq(10));

        // Verify middle line (midLine = 1) background bounds:
        // left = LINE_LEFT - HORIZONTAL_PADDING = 0 - 2 = -2.
        // top = midLine * TEXT_HEIGHT = 1 * 10 = 10.
        // right = LINE_RIGHT + HORIZONTAL_PADDING = 60 + 2 = 62.
        // bottom = (midLine + 1) * TEXT_HEIGHT = 2 * 10 = 20.
        verify(mDrawable, times(1)).setBounds(eq(-2), eq(10), eq(62), eq(20));

        // Verify last line background bounds:
        // left = LINE_LEFT - HORIZONTAL_PADDING = 0 - 2 = -2.
        // top = endLine * TEXT_HEIGHT = 2 * 10 = 20.
        // right = primaryHorizontalEnd + HORIZONTAL_PADDING = 10 + 2 = 12.
        // bottom = (endLine + 1) * TEXT_HEIGHT = 3 * 10 = 30.
        verify(mDrawable, times(1)).setBounds(eq(-2), eq(20), eq(12), eq(30));

        verify(mDrawable, times(3)).draw(mCanvas);
    }

    @Test
    public void testMultiLineSpan_Rtl() {
        final TestTextSpanSpec spec =
                new TestTextSpanSpec(
                        /* spanStart= */ 5,
                        /* spanEnd= */ 15,
                        /* startLine= */ 0,
                        /* endLine= */ 2,
                        /* primaryHorizontalStart= */ 10,
                        /* primaryHorizontalEnd= */ 50,
                        /* rtl= */ true);
        setupText(spec);
        mSpanBackgroundHelper.drawFocusedSpanBackground(mCanvas, mText, mClickableSpan, mLayout);

        // Verify first line background bounds:
        // left = LINE_LEFT - HORIZONTAL_PADDING = 0 - 2 = -2.
        // top = startLine * TEXT_HEIGHT = 0 * 10 = 0.
        // right = primaryHorizontalStart + HORIZONTAL_PADDING = 10 + 2 = 12.
        // bottom = (startLine + 1) * TEXT_HEIGHT = 1 * 10 = 10.
        verify(mDrawable).setBounds(eq(-2), eq(0), eq(12), eq(10));

        // Verify middle line (midLine = 1) background bounds:
        // left = LINE_LEFT - HORIZONTAL_PADDING = 0 - 2 = -2.
        // top = midLine * TEXT_HEIGHT = 1 * 10 = 10.
        // right = LINE_RIGHT + HORIZONTAL_PADDING = 60 + 2 = 62.
        // bottom = (midLine + 1) * TEXT_HEIGHT = 2 * 10 = 20.
        verify(mDrawable).setBounds(eq(-2), eq(10), eq(62), eq(20));

        // Verify last line background bounds:
        // left = primaryHorizontalEnd - HORIZONTAL_PADDING = 50 - 2 = 48.
        // top = endLine * TEXT_HEIGHT = 2 * 10 = 20.
        // right = LINE_RIGHT + HORIZONTAL_PADDING = 60 + 2 = 62.
        // bottom = (endLine + 1) * TEXT_HEIGHT = 3 * 10 = 30.
        verify(mDrawable).setBounds(eq(48), eq(20), eq(62), eq(30));

        verify(mDrawable, times(3)).draw(mCanvas);
    }

    private void setupText(TestTextSpanSpec spec) {
        when(mLayout.getParagraphDirection(spec.mStartLine))
                .thenReturn(spec.mRtl ? Layout.DIR_RIGHT_TO_LEFT : Layout.DIR_LEFT_TO_RIGHT);

        when(mText.getSpanStart(mClickableSpan)).thenReturn(spec.mSpanStart);
        when(mText.getSpanEnd(mClickableSpan)).thenReturn(spec.mSpanEnd);
        when(mLayout.getLineForOffset(spec.mSpanStart)).thenReturn(spec.mStartLine);
        when(mLayout.getLineForOffset(spec.mSpanEnd)).thenReturn(spec.mEndLine);
        when(mLayout.getPrimaryHorizontal(spec.mSpanStart))
                .thenReturn(spec.mPrimaryHorizontalStart);
        when(mLayout.getPrimaryHorizontal(spec.mSpanEnd)).thenReturn(spec.mPrimaryHorizontalEnd);

        for (int i = spec.mStartLine; i <= spec.mEndLine; i++) {
            when(mLayout.getLineTop(i)).thenReturn(i * TEXT_HEIGHT);
            when(mLayout.getLineBottom(i)).thenReturn((i + 1) * TEXT_HEIGHT);
        }

        when(mLayout.getLineLeft(anyInt())).thenReturn(LINE_LEFT);
        when(mLayout.getLineRight(anyInt())).thenReturn(LINE_RIGHT);
    }
}
