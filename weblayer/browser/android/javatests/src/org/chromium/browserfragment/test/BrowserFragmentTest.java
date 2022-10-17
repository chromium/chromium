// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.browserfragment.test;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlockingNoException;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.browserfragment.Browser;
import org.chromium.browserfragment.shell.InstrumentationActivity;

/**
 * Tests that fragment lifecycle works as expected.
 */
@RunWith(BrowserFragmentJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class BrowserFragmentTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private Browser mBrowser;

    @Before
    public void setUp() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShell();
        mBrowser = activity.getBrowserFuture().get();
    }

    @Test
    @SmallTest
    public void successfullyCreateFragment() {
        Assert.assertNotNull(mBrowser);
        Assert.assertNotNull(runOnUiThreadBlockingNoException(() -> mBrowser.createFragment()));
    }
}
