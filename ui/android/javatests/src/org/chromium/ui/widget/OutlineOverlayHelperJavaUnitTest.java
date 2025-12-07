// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.graphics.drawable.Drawable;
import android.view.ViewGroup;
import android.widget.Button;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.filters.SmallTest;

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
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.ui.R;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.RenderTestRule.Component;

import java.io.IOException;

@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class OutlineOverlayHelperJavaUnitTest {

    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> sActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(Component.UI_BROWSER_MOBILE)
                    .build();

    private static BlankUiTestActivity sActivity;

    private Button mButtonGreen;
    private Button mButtonRed;
    private ViewGroup mParentView;
    private Drawable mOutlineDrawable;
    private OutlineOverlayHelper mGreenOutlineHelper;
    private OutlineOverlayHelper mRedOutlineHelper;

    @BeforeClass
    public static void setupBeforeClass() {
        sActivity = sActivityTestRule.launchActivity(null);
    }

    @Before
    public void setup() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sActivity.setContentView(R.layout.focus_outline_test_layout);

                    mParentView = sActivity.findViewById(R.id.button_container);
                    mButtonGreen = sActivity.findViewById(R.id.button_green);
                    mButtonRed = sActivity.findViewById(R.id.button_red);
                    mOutlineDrawable =
                            AppCompatResources.getDrawable(
                                    sActivity, R.drawable.focused_outline_overlay_corners_16dp);

                    mGreenOutlineHelper =
                            new OutlineOverlayHelper(mButtonGreen, mParentView, mOutlineDrawable);
                    mRedOutlineHelper =
                            new OutlineOverlayHelper(mButtonRed, mParentView, mOutlineDrawable);
                });
    }

    @Test
    @SmallTest
    @Feature({"RenderTest"})
    @DisabledTest(message = "https://crbug.com/456125702")
    public void testOutlineVisibilityOnFocusChange() throws IOException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Request focus on the green button.
                    mButtonGreen.requestFocus();

                    // Verify focus states.
                    assertTrue("Green button should have focus.", mButtonGreen.hasFocus());
                    assertFalse("Red button should not have focus.", mButtonRed.hasFocus());

                    // Verify overlay states.
                    assertTrue(
                            "Green outline should be attached.",
                            mGreenOutlineHelper.isOutlineAttachedForTesting());
                    assertFalse(
                            "Red outline should not be attached.",
                            mRedOutlineHelper.isOutlineAttachedForTesting());
                });

        mRenderTestRule.render(mParentView, "green_focused");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Request focus on the red button.
                    mButtonRed.requestFocus();

                    // Verify focus states.
                    assertFalse("Green button should not have focus.", mButtonGreen.hasFocus());
                    assertTrue("Red button should have focus.", mButtonRed.hasFocus());

                    // Verify overlay states.
                    assertFalse(
                            "Green outline should not be attached.",
                            mGreenOutlineHelper.isOutlineAttachedForTesting());
                    assertTrue(
                            "Red outline should be attached.",
                            mRedOutlineHelper.isOutlineAttachedForTesting());
                });

        mRenderTestRule.render(mParentView, "red_focused");
    }
}
