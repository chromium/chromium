// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.text;

import android.text.SpannableString;

import androidx.annotation.Nullable;

import java.util.Arrays;

/**
 * Applies spans to an HTML-looking string and returns the resulting SpannableString.
 * Note: This does not support duplicate, nested or overlapping spans.
 *
 * Example:
 *
 *   String input = "Click to view the <tos>terms of service</tos> or <pn>privacy notice</pn>";
 *   ClickableSpan tosSpan = ...;
 *   ClickableSpan privacySpan = ...;
 *   SpannableString output = SpanApplier.applySpans(input,
 *           new Span("<tos>", "</tos>", tosSpan), new Span("<pn>", "</pn>", privacySpan));
 */
public class SpanApplier {
    private static final int INVALID_INDEX = -1;

    /** Associates a span with the range of text between a start and an end tag. */
    public static final class SpanInfo implements Comparable<SpanInfo> {
        final String mStartTag;
        final String mEndTag;
        final @Nullable Object[] mSpans;
        int mStartTagIndex;
        int mEndTagIndex;

        /**
         * @param startTag The start tag, e.g. "<tos>".
         * @param endTag The end tag, e.g. "</tos>".
         * @param span The span to apply to the text between the start and end tags. May be null,
         *         then SpanApplier will not apply any span.
         */
        public SpanInfo(String startTag, String endTag, @Nullable Object span) {
            mStartTag = startTag;
            mEndTag = endTag;
            mSpans = span == null ? null : new Object[] {span};
        }

        /**
         * @param startTag The start tag, e.g. "<tos>".
         * @param endTag The end tag, e.g. "</tos>".
         * @param spans A vararg list of spans to be applied.
         */
        public SpanInfo(String startTag, String endTag, Object... spans) {
            mStartTag = startTag;
            mEndTag = endTag;
            mSpans = spans;
        }

        @Override
        public int compareTo(SpanInfo other) {
            return this.mStartTagIndex < other.mStartTagIndex
                    ? -1
                    : (this.mStartTagIndex == other.mStartTagIndex ? 0 : 1);
        }

        @Override
        public boolean equals(Object other) {
            if (!(other instanceof SpanInfo)) return false;

            return compareTo((SpanInfo) other) == 0;
        }

        @Override
        public int hashCode() {
            return 0;
        }
    }

    /**
     * Applies spans to an HTML-looking string and returns the resulting SpannableString.
     * If a span cannot be applied (e.g. because the start tag isn't in the input string), then
     * a RuntimeException will be thrown. Nested or duplicated spans are also regarded as an error.
     *
     * @param input The input string.
     * @param spans The Spans which will be applied to the string.
     * @return A SpannableString with the given spans applied.
     * @throws IllegalArgumentException if the span cannot be applied.
     */
    public static SpannableString applySpans(String input, SpanInfo... spans) {
        setSpanInfoIndices(input, spans);

        // Copy the input text to the output, but omit the start and end tags.
        // Update startTagIndex and endTagIndex for each Span as we go.
        int inputIndex = 0;
        StringBuilder output = new StringBuilder(input.length());

        for (SpanInfo span : spans) {
            validateSpanInfo(span, input, inputIndex);
            output.append(input, inputIndex, span.mStartTagIndex);
            inputIndex = span.mStartTagIndex + span.mStartTag.length();
            span.mStartTagIndex = output.length();

            output.append(input, inputIndex, span.mEndTagIndex);
            inputIndex = span.mEndTagIndex + span.mEndTag.length();
            span.mEndTagIndex = output.length();
        }
        output.append(input, inputIndex, input.length());

        SpannableString spannableString = new SpannableString(output);
        for (SpanInfo span : spans) {
            if (span.mStartTagIndex == INVALID_INDEX
                    || span.mSpans == null
                    || span.mSpans.length == 0) {
                continue;
            }

            for (Object s : span.mSpans) {
                if (s == null) continue;
                spannableString.setSpan(s, span.mStartTagIndex, span.mEndTagIndex, 0);
            }
        }

        return spannableString;
    }

    /**
     * Sets up the given {@link SpanInfo} entries to index into the given input and
     * sorted by appearance order.
     */
    private static void setSpanInfoIndices(String input, SpanInfo... spans) {
        for (SpanInfo span : spans) {
            // Set the start/end indices, and if not found, set to INVALID_INDEX.
            span.mStartTagIndex = input.indexOf(span.mStartTag);
            span.mEndTagIndex =
                    input.indexOf(span.mEndTag, span.mStartTagIndex + span.mStartTag.length());
        }

        // Sort the spans from first to last in the order they appear in the input string.
        Arrays.sort(spans);
    }

    /**
     * Validate the given span making sure there are start and end tags that index before
     * the input limit.
     * @param span SpanInfo object defining one span.
     * @param input Input string containing the span.
     * @param spanIndexLimit The mini start position the given span can have in the input string.
     * @throws IllegalArgumentException if the span is not valid.
     */
    private static void validateSpanInfo(SpanInfo span, String input, int spanIndexLimit) {
        // Fail if there is a span without a start or end tag or if there are nested
        // or overlapping spans.
        if (span.mStartTagIndex == INVALID_INDEX
                || span.mEndTagIndex == INVALID_INDEX
                || span.mStartTagIndex < spanIndexLimit) {
            span.mStartTagIndex = -1;
            String error =
                    String.format(
                            "Input string is missing tags %s%s: %s",
                            span.mStartTag, span.mEndTag, input);
            throw new IllegalArgumentException(error);
        }
    }

    /**
     * Removes spans defined as an HTML-looking string from the input string. Note that it is
     * NOT the attribute defined by the SpanInfo but the actual text itself that is removed.
     * If a span cannot be removed (e.g. because the start tag isn't in the input string), then
     * a RuntimeException will be thrown. Nested or duplicated spans are also regarded as an error.
     *
     * @param input The input string.
     * @param spans The Spans which will be removed from the string.
     * @return A String with the given spans all removed.
     * @throws IllegalArgumentException if the span cannot be found.
     */
    public static String removeSpanText(String input, SpanInfo... spans) {
        setSpanInfoIndices(input, spans);

        // Copy the input text to the output, but omit span text including the start and end tags.
        int inputIndex = 0;
        StringBuilder output = new StringBuilder(input.length());

        for (SpanInfo span : spans) {
            validateSpanInfo(span, input, inputIndex);
            output.append(input, inputIndex, span.mStartTagIndex);
            inputIndex = span.mEndTagIndex + span.mEndTag.length();
        }
        output.append(input, inputIndex, input.length());
        return output.toString();
    }
}
