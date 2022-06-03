// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests that embedding support works as expected.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class RenderingTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    @Test
    @SmallTest
    public void testSetSupportEmbeddingFromCallback() {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");

        BoundedCountDownLatch latch = new BoundedCountDownLatch(1);
        String url = "data:text,foo";

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            activity.getBrowser().setSupportsEmbedding(true, (Boolean result) -> {
                Assert.assertTrue(result);
                activity.getBrowser().setSupportsEmbedding(false, (Boolean result2) -> {
                    Assert.assertTrue(result2);
                    latch.countDown();
                });
            });
        });

        latch.timedAwait();
        mActivityTestRule.navigateAndWait(url);
    }

    @Test
    @SmallTest
    public void testRepeatSetSupportEmbeddingGeneratesCallback() {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");

        BoundedCountDownLatch latch = new BoundedCountDownLatch(2);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            activity.getBrowser().setSupportsEmbedding(true, (Boolean result) -> {
                Assert.assertTrue(result);
                latch.countDown();
            });
            activity.getBrowser().setSupportsEmbedding(true, (Boolean result) -> {
                Assert.assertTrue(result);
                latch.countDown();
            });
        });

        latch.timedAwait();
    }
}
