// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.content.Context;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;

/** Unit tests for {@link UiUtils}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class UiUtilsTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> activityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
    }

    /** Test the method {@link UiUtils#maybeSetLinkMovementMethod(TextView)}. */
    @Test
    @SmallTest
    public void testMaybeSetLinkMovementMethod() {
        TextView textView = new TextView(mContext);

        UiUtils.maybeSetLinkMovementMethod(textView);
        Assert.assertNull("No movement method if no text", textView.getMovementMethod());

        textView.setText("test");
        UiUtils.maybeSetLinkMovementMethod(textView);
        Assert.assertNull("No movement method if no clickable span", textView.getMovementMethod());

        textView.setText(
                SpanApplier.applySpans(
                        "test <link> link </link>",
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new ChromeClickableSpan(mContext, (v) -> {}))));
        UiUtils.maybeSetLinkMovementMethod(textView);
        Assert.assertNotNull(
                "Set movement method if a clickable span is included",
                textView.getMovementMethod());
    }
}
