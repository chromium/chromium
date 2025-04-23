// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.text.Layout;
import android.text.Spanned;
import android.text.style.ClickableSpan;

import org.chromium.base.MathUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** A helper class to draw backgrounds for focused {@link ClickableSpan}'s in text. */
@NullMarked
class SpanBackgroundHelper {
    private final int mHorizontalPadding;
    private final Drawable mDrawable;

    /**
     * @param horizontalPadding The horizontal padding in px, that will be applied to the focused
     *     span background.
     * @param drawable The drawable that will be used as the focused span background.
     */
    SpanBackgroundHelper(int horizontalPadding, Drawable drawable) {
        mHorizontalPadding = horizontalPadding;
        mDrawable = drawable;
    }

    /**
     * Draw a background for a {@link ClickableSpan} present in text when it gains keyboard focus.
     *
     * @param canvas The text {@link Canvas} to draw onto.
     * @param text The spanned text containing the {@link ClickableSpan}.
     * @param focusedSpan The {@link ClickableSpan} in {@code text} for which the background will be
     *     drawn.
     * @param layout The {@link Layout} that contains the spanned text.
     */
    void drawFocusedSpanBackground(
            Canvas canvas, Spanned text, ClickableSpan focusedSpan, @Nullable Layout layout) {
        if (layout == null) return;

        int spanStart = text.getSpanStart(focusedSpan);
        int spanEnd = text.getSpanEnd(focusedSpan);
        int startLine = layout.getLineForOffset(spanStart);
        int endLine = layout.getLineForOffset(spanEnd);

        // Start can be on the left or on the right depending on the text direction.
        boolean isRtl = layout.getParagraphDirection(startLine) == Layout.DIR_RIGHT_TO_LEFT;
        int startOffset =
                (int)
                        (layout.getPrimaryHorizontal(spanStart)
                                - MathUtils.flipSignIf(mHorizontalPadding, isRtl));
        int endOffset =
                (int)
                        (layout.getPrimaryHorizontal(spanEnd)
                                + MathUtils.flipSignIf(mHorizontalPadding, isRtl));

        if (startLine == endLine) {
            drawSingleLine(canvas, layout, startLine, startOffset, endOffset);
            return;
        }
        drawMultipleLines(canvas, layout, startLine, endLine, startOffset, endOffset);
    }

    /**
     * Draws a background that starts at {@code startOffset} and ends at {@code endOffset} on a
     * single {@code line}. Note that {@code startOffset} may be greater than {@code endOffset} in
     * an RTL text layout.
     *
     * @param canvas The text {@link Canvas} to draw onto.
     * @param layout The {@link Layout} that contains the text.
     * @param line The line of the text to be highlighted.
     * @param startOffset The offset of the first character of the text to be highlighted.
     * @param endOffset The offset of the last character of the text to be highlighted.
     */
    private void drawSingleLine(
            Canvas canvas, Layout layout, int line, int startOffset, int endOffset) {
        int lineTop = layout.getLineTop(line);
        int lineBottom = layout.getLineBottom(line);

        // Use the min of startOffset/endOffset for left bound, and max of startOffset/endOffset for
        // right bound to ensure valid left/right bounds of the background rect to support both RTL
        // as well as LTR text directions.
        int left = Math.min(startOffset, endOffset);
        int right = Math.max(startOffset, endOffset);

        mDrawable.setBounds(left, lineTop, right, lineBottom);
        mDrawable.draw(canvas);
    }

    /**
     * Draws a background on each line of a multi-line text highlight, accounting for both LTR and
     * RTL text directions.
     *
     * @param canvas The text {@link Canvas} to draw onto.
     * @param layout The {@link Layout} that contains the text.
     * @param startLine The start line of the text to be highlighted.
     * @param endLine The end line of the text to be highlighted.
     * @param startOffset The offset of the first character of the multi-line text to be
     *     highlighted.
     * @param endOffset The offset of the last character of the multi-line text to be highlighted.
     */
    private void drawMultipleLines(
            Canvas canvas,
            Layout layout,
            int startLine,
            int endLine,
            int startOffset,
            int endOffset) {
        boolean isRtl = layout.getParagraphDirection(startLine) == Layout.DIR_RIGHT_TO_LEFT;

        // Draw the first line.
        int startLineEndOffset =
                isRtl
                        ? (int) layout.getLineLeft(startLine) - mHorizontalPadding
                        : (int) layout.getLineRight(startLine) + mHorizontalPadding;
        drawSingleLine(canvas, layout, startLine, startOffset, startLineEndOffset);

        // Draw the middle lines.
        for (int line = startLine + 1; line < endLine; line++) {
            int start = (int) layout.getLineLeft(line) - mHorizontalPadding;
            int end = (int) layout.getLineRight(line) + mHorizontalPadding;
            drawSingleLine(canvas, layout, line, start, end);
        }

        // Draw the last line.
        int endLineStartOffset =
                isRtl
                        ? (int) layout.getLineRight(endLine) + mHorizontalPadding
                        : (int) layout.getLineLeft(endLine) - mHorizontalPadding;
        drawSingleLine(canvas, layout, endLine, endLineStartOffset, endOffset);
    }
}
