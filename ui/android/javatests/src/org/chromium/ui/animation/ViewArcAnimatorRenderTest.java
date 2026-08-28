// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.animation;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import android.animation.Animator;
import android.animation.ValueAnimator;
import android.app.Activity;
import android.graphics.Color;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/** Render tests for {@link CommonAnimationsFactory#createViewArcAnimation}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ViewArcAnimatorRenderTest {
    private static final int ANIMATION_STEPS = 5;
    private static final int CONTAINER_WIDTH = 500;
    private static final int CONTAINER_HEIGHT = 500;
    private static final int VIEW_SIZE = 60;
    private static final float COORD_MIN = 50f;
    private static final float COORD_MAX = 350f;

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE_HUB)
                    .setRevision(1)
                    .build();

    private static Activity sActivity;
    private FrameLayout mRootView;
    private View mView;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() throws Exception {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(false);
        mRenderTestRule.setNightModeEnabled(false);
        CallbackHelper callbackHelper = new CallbackHelper();

        runOnUiThreadBlocking(
                () -> {
                    mRootView = new FrameLayout(sActivity);
                    sActivity.setContentView(
                            mRootView,
                            new ViewGroup.LayoutParams(CONTAINER_WIDTH, CONTAINER_HEIGHT));
                    mView = new View(sActivity.getApplicationContext());
                    mRootView.addView(mView, VIEW_SIZE, VIEW_SIZE);
                    mView.setBackgroundColor(Color.BLUE);
                    mView.post(callbackHelper::notifyCalled);
                });

        // Make sure layout pass occurs
        callbackHelper.waitForOnly();
        CriteriaHelper.pollUiThread(() -> mRootView.isLaidOut());
    }

    @After
    public void tearDown() {
        NightModeTestUtils.tearDownNightModeForBlankUiTestActivity();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testQuadrantI_CounterClockwise() throws IOException {
        Animator animator =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                CommonAnimationsFactory.createViewArcAnimation(
                                        mView,
                                        COORD_MAX,
                                        COORD_MAX,
                                        COORD_MIN,
                                        COORD_MIN,
                                        PathAnimationUtils.ArcDirection.COUNTER_CLOCKWISE));

        RenderTestAnimationUtils.stepThroughAnimation(
                "quadrant_i_counterclockwise",
                mRenderTestRule,
                mRootView,
                (ValueAnimator) animator,
                ANIMATION_STEPS);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testQuadrantI_Clockwise() throws IOException {
        Animator animator =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                CommonAnimationsFactory.createViewArcAnimation(
                                        mView,
                                        COORD_MIN,
                                        COORD_MIN,
                                        COORD_MAX,
                                        COORD_MAX,
                                        PathAnimationUtils.ArcDirection.CLOCKWISE));

        RenderTestAnimationUtils.stepThroughAnimation(
                "quadrant_i_clockwise",
                mRenderTestRule,
                mRootView,
                (ValueAnimator) animator,
                ANIMATION_STEPS);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testQuadrantII_CounterClockwise() throws IOException {
        Animator animator =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                CommonAnimationsFactory.createViewArcAnimation(
                                        mView,
                                        COORD_MAX,
                                        COORD_MIN,
                                        COORD_MIN,
                                        COORD_MAX,
                                        PathAnimationUtils.ArcDirection.COUNTER_CLOCKWISE));

        RenderTestAnimationUtils.stepThroughAnimation(
                "quadrant_ii_counterclockwise",
                mRenderTestRule,
                mRootView,
                (ValueAnimator) animator,
                ANIMATION_STEPS);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testQuadrantII_Clockwise() throws IOException {
        Animator animator =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                CommonAnimationsFactory.createViewArcAnimation(
                                        mView,
                                        COORD_MIN,
                                        COORD_MAX,
                                        COORD_MAX,
                                        COORD_MIN,
                                        PathAnimationUtils.ArcDirection.CLOCKWISE));

        RenderTestAnimationUtils.stepThroughAnimation(
                "quadrant_ii_clockwise",
                mRenderTestRule,
                mRootView,
                (ValueAnimator) animator,
                ANIMATION_STEPS);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testQuadrantIII_CounterClockwise() throws IOException {
        Animator animator =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                CommonAnimationsFactory.createViewArcAnimation(
                                        mView,
                                        COORD_MIN,
                                        COORD_MIN,
                                        COORD_MAX,
                                        COORD_MAX,
                                        PathAnimationUtils.ArcDirection.COUNTER_CLOCKWISE));

        RenderTestAnimationUtils.stepThroughAnimation(
                "quadrant_iii_counterclockwise",
                mRenderTestRule,
                mRootView,
                (ValueAnimator) animator,
                ANIMATION_STEPS);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testQuadrantIII_Clockwise() throws IOException {
        Animator animator =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                CommonAnimationsFactory.createViewArcAnimation(
                                        mView,
                                        COORD_MAX,
                                        COORD_MAX,
                                        COORD_MIN,
                                        COORD_MIN,
                                        PathAnimationUtils.ArcDirection.CLOCKWISE));

        RenderTestAnimationUtils.stepThroughAnimation(
                "quadrant_iii_clockwise",
                mRenderTestRule,
                mRootView,
                (ValueAnimator) animator,
                ANIMATION_STEPS);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testQuadrantIV_CounterClockwise() throws IOException {
        Animator animator =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                CommonAnimationsFactory.createViewArcAnimation(
                                        mView,
                                        COORD_MIN,
                                        COORD_MAX,
                                        COORD_MAX,
                                        COORD_MIN,
                                        PathAnimationUtils.ArcDirection.COUNTER_CLOCKWISE));

        RenderTestAnimationUtils.stepThroughAnimation(
                "quadrant_iv_counterclockwise",
                mRenderTestRule,
                mRootView,
                (ValueAnimator) animator,
                ANIMATION_STEPS);
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testQuadrantIV_Clockwise() throws IOException {
        Animator animator =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                CommonAnimationsFactory.createViewArcAnimation(
                                        mView,
                                        COORD_MAX,
                                        COORD_MIN,
                                        COORD_MIN,
                                        COORD_MAX,
                                        PathAnimationUtils.ArcDirection.CLOCKWISE));

        RenderTestAnimationUtils.stepThroughAnimation(
                "quadrant_iv_clockwise",
                mRenderTestRule,
                mRootView,
                (ValueAnimator) animator,
                ANIMATION_STEPS);
    }
}
