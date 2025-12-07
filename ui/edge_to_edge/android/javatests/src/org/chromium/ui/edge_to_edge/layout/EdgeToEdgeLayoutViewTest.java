// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.edge_to_edge.layout;

import static org.mockito.Mockito.doReturn;

import static org.chromium.base.test.util.Batch.PER_CLASS;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.Rect;
import android.view.View;
import android.widget.FrameLayout;

import androidx.core.graphics.Insets;
import androidx.core.view.DisplayCutoutCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.ui.edge_to_edge.EdgeToEdgeFieldTrial;
import org.chromium.ui.edge_to_edge.R;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.IOException;

/** Java unit test for {@link EdgeToEdgeBaseLayout} */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(PER_CLASS) // Tests changes the content view of the activity.
public class EdgeToEdgeLayoutViewTest {
    @ClassRule
    public static final BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private static Activity sActivity;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public RenderTestRule mRenderTestRule =
            new RenderTestRule.Builder()
                    .setCorpus(RenderTestRule.Corpus.ANDROID_RENDER_TESTS_PUBLIC)
                    .setBugComponent(Component.UI_BROWSER_MOBILE_EDGE_TO_EDGE)
                    .setRevision(1)
                    .build();

    private static final int STATUS_BAR_SIZE = 100;
    private static final int NAV_BAR_SIZE = 150;
    private static final int DISPLAY_CUTOUT_SIZE = 75;
    private static final int IME_SIZE = 300;

    private static final int STATUS_BAR_COLOR = Color.RED;
    private static final int NAV_BAR_COLOR = Color.GREEN;
    private static final int NAV_BAR_DIVIDER_COLOR = Color.BLUE;
    private static final int BG_COLOR = Color.GRAY;

    @Mock private EdgeToEdgeFieldTrial mUseBackupNavbarInsetsFieldTrial;

    private EdgeToEdgeLayoutCoordinator mEdgeToEdgeLayoutCoordinator;
    private FrameLayout mContentView;
    private View mEdgeToEdgeLayout;

    @BeforeClass
    public static void setupSuite() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setUp() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    doReturn(true)
                            .when(mUseBackupNavbarInsetsFieldTrial)
                            .isEnabledForManufacturerVersion();

                    mEdgeToEdgeLayoutCoordinator =
                            new EdgeToEdgeLayoutCoordinator(
                                    sActivity,
                                    null,
                                    /* useBackupNavbarInsetsEnabled= */ true,
                                    /* useBackupNavbarInsetsFieldTrial= */ mUseBackupNavbarInsetsFieldTrial,
                                    /* canUseMandatoryGesturesInsets= */ true);

                    mContentView = new FrameLayout(sActivity, null);
                    sActivity.setContentView(
                            mEdgeToEdgeLayoutCoordinator.wrapContentView(mContentView));

                    mEdgeToEdgeLayout = sActivity.findViewById(R.id.edge_to_edge_base_layout);

                    // Set colors to be used in render tests.
                    mContentView.setBackgroundColor(BG_COLOR);
                    mEdgeToEdgeLayoutCoordinator.setStatusBarColor(STATUS_BAR_COLOR);
                    mEdgeToEdgeLayoutCoordinator.setNavigationBarColor(NAV_BAR_COLOR);
                    mEdgeToEdgeLayoutCoordinator.setNavigationBarDividerColor(
                            NAV_BAR_DIVIDER_COLOR);
                });
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void renderBottomNavBar() throws IOException {
        WindowInsetsCompat topBottomInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(
                                WindowInsetsCompat.Type.statusBars(),
                                Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(
                                WindowInsetsCompat.Type.navigationBars(),
                                Insets.of(0, 0, 0, NAV_BAR_SIZE))
                        .build();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(mContentView, topBottomInsets);
                });
        CriteriaHelper.pollUiThread(() -> !mEdgeToEdgeLayout.isDirty());
        mRenderTestRule.render(mEdgeToEdgeLayout, "bottom_nav_bar");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void renderLeftNavBar() throws IOException {
        WindowInsetsCompat topLeftInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(
                                WindowInsetsCompat.Type.statusBars(),
                                Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(
                                WindowInsetsCompat.Type.navigationBars(),
                                Insets.of(NAV_BAR_SIZE, 0, 0, 0))
                        .build();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(mContentView, topLeftInsets);
                });

        CriteriaHelper.pollUiThread(() -> !mEdgeToEdgeLayout.isDirty());
        mRenderTestRule.render(mEdgeToEdgeLayout, "left_nav_Bar");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "https://crbug.com/431608564")
    public void renderDisplayCutoutOverlapSystemBars() throws IOException {
        WindowInsetsCompat topBottomSysBarsWithLeftCutoutInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(
                                WindowInsetsCompat.Type.statusBars(),
                                Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(
                                WindowInsetsCompat.Type.navigationBars(),
                                Insets.of(0, 0, 0, NAV_BAR_SIZE))
                        .setInsets(
                                WindowInsetsCompat.Type.displayCutout(),
                                Insets.of(DISPLAY_CUTOUT_SIZE, 0, 0, 0))
                        .setDisplayCutout(
                                new DisplayCutoutCompat(
                                        new Rect(DISPLAY_CUTOUT_SIZE, 0, 0, 0), null))
                        .build();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(
                            mContentView, topBottomSysBarsWithLeftCutoutInsets);
                });

        CriteriaHelper.pollUiThread(() -> !mEdgeToEdgeLayout.isDirty());
        mRenderTestRule.render(mEdgeToEdgeLayout, "left_display_cutout_overlap");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void renderDisplayCutoutOverlapStatusBarOnly() throws IOException {
        WindowInsetsCompat topLeftSysBarsRightCutoutInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(
                                WindowInsetsCompat.Type.statusBars(),
                                Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(
                                WindowInsetsCompat.Type.navigationBars(),
                                Insets.of(NAV_BAR_SIZE, 0, 0, 0))
                        .setInsets(
                                WindowInsetsCompat.Type.displayCutout(),
                                Insets.of(0, 0, DISPLAY_CUTOUT_SIZE, 0))
                        .setDisplayCutout(
                                new DisplayCutoutCompat(
                                        new Rect(0, 0, DISPLAY_CUTOUT_SIZE, 0), null))
                        .build();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(
                            mContentView, topLeftSysBarsRightCutoutInsets);
                });

        CriteriaHelper.pollUiThread(() -> !mEdgeToEdgeLayout.isDirty());
        mRenderTestRule.render(mEdgeToEdgeLayout, "top_left_sys_bars_right_cutout");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void renderImeInsets() throws IOException {
        WindowInsetsCompat topLeftSysBarsRightCutoutInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(
                                WindowInsetsCompat.Type.statusBars(),
                                Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(
                                WindowInsetsCompat.Type.navigationBars(),
                                Insets.of(0, 0, 0, NAV_BAR_SIZE))
                        .setInsets(WindowInsetsCompat.Type.ime(), Insets.of(0, 0, 0, IME_SIZE))
                        .build();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(
                            mContentView, topLeftSysBarsRightCutoutInsets);
                });

        CriteriaHelper.pollUiThread(() -> !mEdgeToEdgeLayout.isDirty());
        mRenderTestRule.render(mEdgeToEdgeLayout, "top_bottom_ime");
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    public void renderFullscreenWhenTappableNavigation() throws IOException {
        WindowInsetsCompat fullscreenWhenTappableNavInsets =
                new WindowInsetsCompat.Builder()
                        .setInsets(WindowInsetsCompat.Type.statusBars(), Insets.of(0, 0, 0, 0))
                        .setInsetsIgnoringVisibility(
                                WindowInsetsCompat.Type.statusBars(),
                                Insets.of(0, STATUS_BAR_SIZE, 0, 0))
                        .setInsets(WindowInsetsCompat.Type.navigationBars(), Insets.of(0, 0, 0, 0))
                        .setInsetsIgnoringVisibility(
                                WindowInsetsCompat.Type.navigationBars(),
                                Insets.of(0, 0, 0, NAV_BAR_SIZE))
                        .setInsets(
                                WindowInsetsCompat.Type.tappableElement(),
                                Insets.of(0, 0, 0, NAV_BAR_SIZE))
                        .build();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mEdgeToEdgeLayoutCoordinator.onApplyWindowInsets(
                            mContentView, fullscreenWhenTappableNavInsets);
                });

        CriteriaHelper.pollUiThread(() -> !mEdgeToEdgeLayout.isDirty());
        mRenderTestRule.render(mEdgeToEdgeLayout, "fullscreen_when_tappable");
    }
}
