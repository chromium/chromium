// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.text;

import android.text.SpannableString;
import android.text.style.BulletSpan;
import android.text.style.QuoteSpan;
import android.text.style.ScaleXSpan;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.text.SpanApplier.SpanInfo;

/** Tests public methods in SpanApplier. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SpanApplierTest {
    @Test
    public void testApplySpan() {
        String input = "Lorem ipsum <span>dolor</span> sit amet.";
        String output = "Lorem ipsum dolor sit amet.";
        SpanInfo span = new SpanInfo("<span>", "</span>", new QuoteSpan());

        SpannableString expectedOutput = new SpannableString(output);
        expectedOutput.setSpan(span.mSpans[0], 12, 17, 0);
        SpannableString actualOutput = SpanApplier.applySpans(input, span);

        assertSpannableStringEquality(expectedOutput, actualOutput);
    }

    @Test
    public void testApplyMultipleSpans() {
        String input =
                "Lorem <link>ipsum</link> dolor sit amet, <cons>consectetur adipiscing</cons>"
                        + " <elit>elit. Proin<endElit> consectetur.";
        String output =
                "Lorem ipsum dolor sit amet, " + "consectetur adipiscing elit. Proin consectetur.";
        SpanInfo linkSpan = new SpanInfo("<link>", "</link>", new QuoteSpan());
        SpanInfo consSpan = new SpanInfo("<cons>", "</cons>", new BulletSpan());
        SpanInfo elitSpan = new SpanInfo("<elit>", "<endElit>", new ScaleXSpan(1));

        SpannableString expectedOutput = new SpannableString(output);
        expectedOutput.setSpan(linkSpan.mSpans[0], 6, 11, 0);
        expectedOutput.setSpan(consSpan.mSpans[0], 28, 50, 0);
        expectedOutput.setSpan(elitSpan.mSpans[0], 51, 62, 0);
        SpannableString actualOutput = SpanApplier.applySpans(input, elitSpan, consSpan, linkSpan);

        assertSpannableStringEquality(expectedOutput, actualOutput);
    }

    @Test
    public void testVarargSpanInfoConstructor() {
        String input = "Lorem ipsum <span>dolor</span> sit amet.";
        String output = "Lorem ipsum dolor sit amet.";
        SpanInfo multiSpan = new SpanInfo("<span>", "</span>", new QuoteSpan(), new BulletSpan());

        SpannableString expectedOutput = new SpannableString(output);
        expectedOutput.setSpan(multiSpan.mSpans[0], 12, 17, 0);
        expectedOutput.setSpan(multiSpan.mSpans[1], 12, 17, 0);
        SpannableString actualOutput = SpanApplier.applySpans(input, multiSpan);

        assertSpannableStringEquality(expectedOutput, actualOutput);
    }

    @Test
    public void testEndTagMissingInInput() {
        String input = "Lorem ipsum <span>dolor</> sit amet.";
        SpanInfo span = new SpanInfo("<span>", "</span>", new QuoteSpan());

        try {
            SpanApplier.applySpans(input, span);
            Assert.fail("Expected IllegalArgumentException to be thrown.");
        } catch (IllegalArgumentException e) {
            // success
        }
    }

    @Test
    public void testStartTagMissingInInput() {
        String input = "Lorem ipsum <>dolor</span> sit amet.";
        SpanInfo span = new SpanInfo("<span>", "</span>", new QuoteSpan());

        try {
            SpanApplier.applySpans(input, span);
            Assert.fail("Expected IllegalArgumentException to be thrown.");
        } catch (IllegalArgumentException e) {
            // success
        }
    }

    @Test
    public void testNestedTagsInInput() {
        String input = "Lorem ipsum <span>dolor<span2> sit </span2> </span> amet.";
        SpanInfo span = new SpanInfo("<span>", "</span>", new QuoteSpan());
        SpanInfo span2 = new SpanInfo("<span2>", "</span2>", new QuoteSpan());

        try {
            SpanApplier.applySpans(input, span, span2);
            Assert.fail("Expected IllegalArgumentException to be thrown.");
        } catch (IllegalArgumentException e) {
            // success
        }
    }

    @Test
    public void testDuplicateTagsInInput() {
        String input = "Lorem ipsum <span>dolor</span> <span>sit </span> amet.";
        SpanInfo span = new SpanInfo("<span>", "</span>", new QuoteSpan());
        SpanInfo span2 = new SpanInfo("<span>", "</span>", new QuoteSpan());

        try {
            SpanApplier.applySpans(input, span, span2);
            Assert.fail("Expected IllegalArgumentException to be thrown.");
        } catch (IllegalArgumentException e) {
            // success
        }
    }

    @Test
    public void testNullSpan() {
        String input = "Lorem <link>ipsum</link> dolor <span>sit</span> amet.";
        SpanInfo linkSpan = new SpanInfo("<link>", "</link>", new QuoteSpan());
        SpanInfo nullSpan = new SpanInfo("<span>", "</span>", (Object) null);

        String output = "Lorem ipsum dolor sit amet.";
        SpannableString expectedOutput = new SpannableString(output);
        expectedOutput.setSpan(linkSpan.mSpans[0], 6, 11, 0);
        SpannableString actualOutput = SpanApplier.applySpans(input, linkSpan, nullSpan);

        assertSpannableStringEquality(expectedOutput, actualOutput);
    }

    @Test
    public void testRemoveSpanText() {
        String input = "<cons>consectetur adipiscing</cons>";
        String expected = "";
        SpanInfo span = new SpanInfo("<cons>", "</cons>");

        String result = SpanApplier.removeSpanText(input, span);
        Assert.assertEquals(expected, result);
    }

    @Test
    public void testRemoveMultipleSpanText() {
        String input =
                "Lorem <link>ipsum</link> dolor sit amet, <cons>consectetur"
                        + " adipiscing</cons><elit>elit. Proin<endElit> consectetur.";
        String expected = "Lorem " + " dolor sit amet, " + " consectetur.";
        SpanInfo linkSpan = new SpanInfo("<link>", "</link>");
        SpanInfo consSpan = new SpanInfo("<cons>", "</cons>");
        SpanInfo elitSpan = new SpanInfo("<elit>", "<endElit>");

        // Span appearance order does not matter.
        String result = SpanApplier.removeSpanText(input, consSpan, linkSpan, elitSpan);
        Assert.assertEquals(expected, result);
    }

    @Test
    public void testRemoveNestedSpans() {
        String input = "<cons><link>ipsum</link>consectetur adipiscing</cons> dolor sit amet";
        SpanInfo linkSpan = new SpanInfo("<link>", "</link>");
        SpanInfo consSpan = new SpanInfo("<cons>", "</cons>");

        try {
            SpanApplier.removeSpanText(input, linkSpan, consSpan);
            Assert.fail("Expected IllegalArgumentException to be thrown.");
        } catch (IllegalArgumentException e) {
            // success
        }
    }

    /*
     * Tests the attributes of two SpannableStrings and asserts expected equality.
     */
    private void assertSpannableStringEquality(SpannableString expected, SpannableString actual) {
        if (!expected.equals(actual)) {
            Assert.fail(
                    "Expected string is "
                            + getSpannableStringDescription(expected)
                            + " Actual string is "
                            + getSpannableStringDescription(actual));
        }
    }

    private String getSpannableStringDescription(SpannableString spannableString) {
        Object[] spans = spannableString.getSpans(0, spannableString.length(), Object.class);
        StringBuilder description = new StringBuilder();
        description.append("\"" + spannableString + "\"" + " with spans: ");
        for (int i = 0; i < spans.length; i++) {
            Object span = spans[i];
            description.append(
                    span.getClass().getName()
                            + " from "
                            + spannableString.getSpanStart(span)
                            + " to "
                            + spannableString.getSpanEnd(span));
            if (i != spans.length - 1) {
                description.append(", ");
            }
        }

        description.append('.');
        return description.toString();
    }
}
