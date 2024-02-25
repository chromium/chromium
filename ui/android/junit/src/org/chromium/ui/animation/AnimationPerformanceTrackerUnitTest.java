// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.os.SystemClock;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** JUnit tests for {@link AnimationPerformanceTracker}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowSystemClock.class})
public class AnimationPerformanceTrackerUnitTest implements AnimationPerformanceTracker.Listener {
    private static final long INITIAL_TIME = 1000;
    private static final long INVALID_TIME = -1L;
    private static final float TOLERANCE = 0.001f;

    private AnimationPerformanceTracker mTracker;
    private AnimationPerformanceTracker.AnimationMetrics mMetrics;

    @Before
    public void setUp() {
        SystemClock.setCurrentTimeMillis(INITIAL_TIME);
        mTracker = new AnimationPerformanceTracker();
        mTracker.addListener(this);
    }

    @After
    public void tearDown() {
        mTracker.removeListener(this);
    }

    @Test
    @SmallTest
    public void testStartEndNoUpdate() {
        assertNull(mMetrics);

        mTracker.onStart();
        final long timeDeltaMs = 2000;
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + timeDeltaMs);
        mTracker.onEnd();

        assertEquals("Unexpected start time", INITIAL_TIME, mMetrics.getStartTimeMs());
        assertEquals("Unexpected first frame time", INVALID_TIME, mMetrics.getFirstFrameTimeMs());
        assertEquals(
                "Unexpected first frame latency", INVALID_TIME, mMetrics.getFirstFrameLatencyMs());
        assertEquals("Unexpected last frame time", INVALID_TIME, mMetrics.getLastFrameTimeMs());
        assertEquals(
                "Unexpected max frame interval", INVALID_TIME, mMetrics.getMaxFrameIntervalMs());
        assertEquals("Unexpected frame count", 0, mMetrics.getFrameCount());
        assertEquals("Unexpected elapsed time", timeDeltaMs, mMetrics.getElapsedTimeMs());
        assertEquals("Unexpected fps", 0f, mMetrics.getFramesPerSecond(), TOLERANCE);
    }

    @Test
    @SmallTest
    public void testStartUpdateEnd() {
        assertNull(mMetrics);

        mTracker.onStart();
        final long time1DeltaMs = 20;
        final long time1 = INITIAL_TIME + time1DeltaMs;
        SystemClock.setCurrentTimeMillis(time1);
        mTracker.onUpdate();
        final long time2DeltaMs = 10;
        final long time2 = time1 + time2DeltaMs;
        SystemClock.setCurrentTimeMillis(time2);
        mTracker.onUpdate();
        mTracker.onEnd();

        assertEquals("Unexpected start time", INITIAL_TIME, mMetrics.getStartTimeMs());
        assertEquals("Unexpected first frame time", time1, mMetrics.getFirstFrameTimeMs());
        assertEquals(
                "Unexpected first frame latency", time1DeltaMs, mMetrics.getFirstFrameLatencyMs());
        assertEquals("Unexpected last frame time", time2, mMetrics.getLastFrameTimeMs());
        assertEquals(
                "Unexpected max frame interval", time1DeltaMs, mMetrics.getMaxFrameIntervalMs());
        assertEquals("Unexpected frame count", 2, mMetrics.getFrameCount());
        assertEquals(
                "Unexpected elapsed time",
                time1DeltaMs + time2DeltaMs,
                mMetrics.getElapsedTimeMs());
        assertEquals(
                "Unexpected fps",
                2f * 1000f / (time1DeltaMs + time2DeltaMs),
                mMetrics.getFramesPerSecond(),
                TOLERANCE);
    }

    @Override
    public void onAnimationEnd(AnimationPerformanceTracker.AnimationMetrics metrics) {
        mMetrics = metrics;
    }
}
