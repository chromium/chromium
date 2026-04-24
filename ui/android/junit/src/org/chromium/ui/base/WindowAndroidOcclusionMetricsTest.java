// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.graphics.Rect;
import android.graphics.Region;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.GraphicsMode;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;

import java.time.Duration;

/** Tests for {@link WindowAndroidOcclusionMetrics}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = BaseRobolectricTestRunner.MAX_SDK)
@GraphicsMode(GraphicsMode.Mode.NATIVE)
public class WindowAndroidOcclusionMetricsTest {

    @Test
    public void testMaxOfMinVisibleRectsDurationMetric_Bucket0() {
        WindowAndroidOcclusionMetrics metrics = new WindowAndroidOcclusionMetrics();

        metrics.onOcclusionStateChanged(true, null);

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.Window.OcclusionExperimental.MaxOfMinVisibleRectsDuration.0",
                        1000);
        ShadowSystemClock.advanceBy(Duration.ofSeconds(1));

        // Changes bucket to FullyVisible, flushing the previous bucket (0) with 1000ms duration.
        metrics.onOcclusionStateChanged(false, null);
        histogramWatcher.assertExpected();

        metrics.onDestroy();
    }

    @Test
    public void testMaxOfMinVisibleRectsDurationMetric_FullyVisible() {
        WindowAndroidOcclusionMetrics metrics = new WindowAndroidOcclusionMetrics();

        metrics.onOcclusionStateChanged(false, null);

        final String expectedHistogramName =
                "Android.Window.OcclusionExperimental.MaxOfMinVisibleRectsDuration.FullyVisible";
        var histogramWatcher = HistogramWatcher.newSingleRecordWatcher(expectedHistogramName, 1000);
        ShadowSystemClock.advanceBy(Duration.ofSeconds(1));

        // Changes bucket to 0, flushing the previous bucket (FullyVisible) with 1000ms duration.
        metrics.onOcclusionStateChanged(true, null);
        histogramWatcher.assertExpected();

        metrics.onDestroy();
    }

    @Test
    public void testMaxOfMinVisibleRectsDurationMetric() {
        WindowAndroidOcclusionMetrics metrics = new WindowAndroidOcclusionMetrics();

        Region region = new Region(0, 0, 100, 200);
        region.op(new Rect(200, 200, 500, 600), Region.Op.UNION);

        // Sets bucket to 201Plus (since min dimension of (200,200)-(500,600) is 300)
        metrics.onOcclusionStateChanged(false, region);

        final String expected201PlusHistogramName =
                "Android.Window.OcclusionExperimental.MaxOfMinVisibleRectsDuration.201Plus";
        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(expected201PlusHistogramName, 1000);
        ShadowSystemClock.advanceBy(Duration.ofSeconds(1));

        // Changes bucket to 101To200, flushing the previous bucket (201Plus) with 1000ms duration.
        Region newRegion = new Region(0, 0, 150, 150);
        metrics.onOcclusionStateChanged(false, newRegion);
        histogramWatcher.assertExpected();

        // Testing that the metric is not recorded again if the bucket is the same.
        final String expected101To200HistogramName =
                "Android.Window.OcclusionExperimental.MaxOfMinVisibleRectsDuration.101To200";
        var emptyWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(expected101To200HistogramName)
                        .build();
        Region sameBucketRegion = new Region(0, 0, 160, 160);
        metrics.onOcclusionStateChanged(false, sameBucketRegion);
        emptyWatcher.assertExpected();

        // Check if destroy flushes the active bucket.
        var destroyWatcher =
                HistogramWatcher.newSingleRecordWatcher(expected101To200HistogramName, 2000);
        ShadowSystemClock.advanceBy(Duration.ofSeconds(2));
        metrics.onDestroy();
        destroyWatcher.assertExpected();
    }
}
