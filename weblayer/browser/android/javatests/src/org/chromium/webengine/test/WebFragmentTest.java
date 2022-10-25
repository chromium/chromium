// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.test;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlockingNoException;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.webengine.WebSandbox;
import org.chromium.webengine.shell.InstrumentationActivity;

/**
 * Tests that fragment lifecycle works as expected.
 */
@RunWith(WebEngineJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class WebFragmentTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private WebSandbox mWebSandbox;

    @Before
    public void setUp() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShell();
        mWebSandbox = activity.getWebSandboxFuture().get();
    }

    @Test
    @SmallTest
    public void successfullyCreateFragment() {
        Assert.assertNotNull(mWebSandbox);
        Assert.assertNotNull(runOnUiThreadBlockingNoException(() -> mWebSandbox.createFragment()));
    }
}
