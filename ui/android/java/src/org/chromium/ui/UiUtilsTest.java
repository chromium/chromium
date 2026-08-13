// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.ListAdapter;
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

import java.util.Set;

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

    /** Test the method {@link UiUtils#computeMenuWidth(int, int, int, int, int)}. */
    @Test
    @SmallTest
    public void testComputeMenuWidth() {
        final int minWidth = 188;
        final int maxAllowedWidth = 800;
        final int margin = 16;

        // Content fits within min and max bounds on a wide screen.
        Assert.assertEquals(
                "Content width within bounds should be preserved",
                300,
                UiUtils.computeMenuWidth(300, minWidth, maxAllowedWidth, margin, 1000));

        // Content smaller than minWidth is clamped up to minWidth.
        Assert.assertEquals(
                "Content width smaller than minWidth should be clamped to minWidth",
                188,
                UiUtils.computeMenuWidth(100, minWidth, maxAllowedWidth, margin, 1000));

        // Content larger than maxAllowedWidth is clamped down to maxAllowedWidth.
        Assert.assertEquals(
                "Content width larger than maxAllowedWidth should be clamped to maxAllowedWidth",
                800,
                UiUtils.computeMenuWidth(900, minWidth, maxAllowedWidth, margin, 1000));

        // Screen width limits maxWidth when available space (600 - 2 * 16 = 568) is less than
        // maxAllowedWidth.
        Assert.assertEquals(
                "Width should be constrained by available screen width",
                568,
                UiUtils.computeMenuWidth(700, minWidth, maxAllowedWidth, margin, 600));

        // Very narrow screen where available space (150 - 2 * 16 = 118) is less than minWidth.
        Assert.assertEquals(
                "Narrow screen space should take precedence over minWidth",
                118,
                UiUtils.computeMenuWidth(300, minWidth, maxAllowedWidth, margin, 150));
    }

    private static class MultiTypeTestAdapter extends BaseAdapter {
        private final Context mContext;
        private final int[] mWidths;
        private final int[] mHeights;
        private final int[] mTypes;

        MultiTypeTestAdapter(Context context, int[] widths, int[] heights, int[] types) {
            mContext = context;
            mWidths = widths;
            mHeights = heights;
            mTypes = types;
        }

        @Override
        public int getCount() {
            return mWidths.length;
        }

        @Override
        public int getViewTypeCount() {
            return 2;
        }

        @Override
        public int getItemViewType(int position) {
            return mTypes[position];
        }

        @Override
        public Object getItem(int position) {
            return position;
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            View view = convertView != null ? convertView : new View(mContext);
            view.setMinimumWidth(mWidths[position]);
            view.setMinimumHeight(mHeights[position]);
            return view;
        }
    }

    /**
     * Test the method {@link UiUtils#computeListAdapterContentDimensions(ListAdapter, ViewGroup,
     * Set)}.
     */
    @Test
    @SmallTest
    public void testComputeListAdapterContentDimensions() {
        MultiTypeTestAdapter adapter =
                new MultiTypeTestAdapter(
                        mContext, new int[] {500, 200}, new int[] {50, 40}, new int[] {0, 1});

        int[] allDimensions = UiUtils.computeListAdapterContentDimensions(adapter, null);
        Assert.assertEquals("Max width should include all items", 500, allDimensions[0]);
        Assert.assertEquals("Height should sum all items", 90, allDimensions[1]);

        int[] excludedDimensions =
                UiUtils.computeListAdapterContentDimensions(adapter, null, Set.of(0));
        Assert.assertEquals("Max width should exclude item of type 0", 200, excludedDimensions[0]);
        Assert.assertEquals(
                "Height should still sum all items even when type 0 is excluded from width",
                90,
                excludedDimensions[1]);
    }
}
