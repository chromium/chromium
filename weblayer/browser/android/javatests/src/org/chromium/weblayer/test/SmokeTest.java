// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.lang.ref.PhantomReference;
import java.lang.ref.Reference;
import java.lang.ref.ReferenceQueue;
import java.util.concurrent.CountDownLatch;

/**
 * Basic tests to make sure WebLayer works as expected.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class SmokeTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    @Test
    @SmallTest
    public void testSetSupportEmbedding() {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { activity.getBrowser().setSupportsEmbedding(true); });

        CountDownLatch latch = new CountDownLatch(1);
        String url = "data:text,foo";

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            activity.getBrowser().setSupportsEmbedding(true).addCallback((Boolean result) -> {
                Assert.assertTrue(result);
                latch.countDown();
            });
        });

        try {
            latch.await();
        } catch (InterruptedException e) {
            Assert.fail(e.toString());
        }
        mActivityTestRule.navigateAndWait(url);
    }

    @Test
    @SmallTest
    public void testActivityShouldNotLeak() {
        ReferenceQueue<InstrumentationActivity> referenceQueue = new ReferenceQueue<>();
        PhantomReference<InstrumentationActivity> reference;
        {
            InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
            mActivityTestRule.recreateActivity();
            boolean destroyed =
                    TestThreadUtils.runOnUiThreadBlockingNoException(() -> activity.isDestroyed());
            Assert.assertTrue(destroyed);

            reference = new PhantomReference<>(activity, referenceQueue);
        }

        Runtime.getRuntime().gc();
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Reference enqueuedReference = referenceQueue.poll();
                if (enqueuedReference == null) {
                    Runtime.getRuntime().gc();
                    return false;
                }
                Assert.assertEquals(reference, enqueuedReference);
                return true;
            }
        });
    }
}
