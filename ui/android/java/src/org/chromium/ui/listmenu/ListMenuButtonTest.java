// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import android.content.Context;
import android.os.Build;
import android.view.MotionEvent;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import java.util.concurrent.TimeoutException;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.ui.R;
import org.chromium.ui.base.MotionEventTestUtils;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Unit tests for {@link ListMenuButton}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ListMenuButtonTest {
    @ClassRule
    public static BaseActivityTestRule<BlankUiTestActivity> activityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
    }

    @Test
    @SmallTest
    public void testA11yLabel() {
        ListMenuButton button = new ListMenuButton(mContext, null);

        button.setContentDescriptionContext("");
        Assert.assertEquals(
                mContext.getString(R.string.accessibility_toolbar_btn_menu),
                button.getContentDescription());

        String title = "Test title";
        button.setContentDescriptionContext(title);
        Assert.assertEquals(
                mContext.getString(R.string.accessibility_list_menu_button, title),
                button.getContentDescription());
    }

    @Test
    @SmallTest
    public void testTriggerShowMenuTwice() {
        ListMenuButton button = new ListMenuButton(mContext, null);
        button.setAttachedToWindowForTesting();
        View view = new View(mContext);
        button.setDelegate(
                () ->
                        new ListMenu() {
                            @Override
                            public View getContentView() {
                                return view;
                            }

                            @Override
                            public void addContentViewClickRunnable(Runnable runnable) {}

                            @Override
                            public int getMaxItemWidth() {
                                return 0;
                            }
                        },
                true);
        // Expect no crash when calling showMenu twice.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    button.showMenu();
                    button.showMenu();
                });
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.R)
    public void testSecondaryClick() {
        ListMenuButton button = new ListMenuButton(mContext, null);
        CallbackHelper longClickHelper = new CallbackHelper();
        button.setOnLongClickListener(
                (v) -> {
                    longClickHelper.notifyCalled();
                    return true;
                });
        MotionEvent secondaryClickEvent = MotionEventTestUtils.getTrackRightClickEvent();
        button.onGenericMotionEvent(secondaryClickEvent);
        try {
            longClickHelper.waitForNext();
        } catch (TimeoutException e) {
            throw new AssertionError("Long click should be performed on secondary click.", e);
        }
    }
}
