// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.support.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests that fragment restore works as expected.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class FragmentRestoreTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    @Test
    @SmallTest
    public void successfullyLoadsUrlAfterRotation() {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        Tab tab = TestThreadUtils.runOnUiThreadBlockingNoException(() -> activity.getTab());

        String url = "data:text,foo";
        mActivityTestRule.navigateAndWait(tab, url, false);

        mActivityTestRule.recreateActivity();

        InstrumentationActivity newActivity = mActivityTestRule.getActivity();
        tab = TestThreadUtils.runOnUiThreadBlockingNoException(() -> newActivity.getTab());
        url = "data:text,bar";
        mActivityTestRule.navigateAndWait(tab, url, false);
    }
}
