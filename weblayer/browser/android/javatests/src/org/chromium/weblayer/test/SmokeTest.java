// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.PixelCopy;

import androidx.annotation.RequiresApi;
import androidx.fragment.app.Fragment;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.lang.ref.PhantomReference;
import java.lang.ref.Reference;
import java.lang.ref.ReferenceQueue;

/**
 * Basic tests to make sure WebLayer works as expected.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class SmokeTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    // Should be used only for solid color pages, since the window is downsampled significantly.
    @RequiresApi(Build.VERSION_CODES.O)
    private int getWindowCenterPixelColor(Activity activity) {
        BoundedCountDownLatch latch = new BoundedCountDownLatch(1);
        Bitmap bitmap = Bitmap.createBitmap(200, 200, Bitmap.Config.ARGB_8888, /*hasAlpha=*/true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            PixelCopy.request(activity.getWindow(), bitmap,
                    (int copyResult) -> { latch.countDown(); }, new Handler(Looper.myLooper()));
        });
        latch.timedAwait();
        int color = bitmap.getPixel(100, 100);
        bitmap.recycle();
        return color;
    }

    private void checkColorIsClose(int color1, int color2, int tolerance) {
        Criteria.checkThat(
                Math.abs(Color.alpha(color1) - Color.alpha(color2)), Matchers.lessThan(tolerance));
        Criteria.checkThat(
                Math.abs(Color.red(color1) - Color.red(color2)), Matchers.lessThan(tolerance));
        Criteria.checkThat(
                Math.abs(Color.green(color1) - Color.green(color2)), Matchers.lessThan(tolerance));
        Criteria.checkThat(
                Math.abs(Color.blue(color1) - Color.blue(color2)), Matchers.lessThan(tolerance));
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(89)
    public void testSetMinimumSurfaceSize() {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { activity.getBrowser().setMinimumSurfaceSize(100, 200); });
        // Nothing to check here.
    }

    @Test
    @SmallTest
    public void testActivityShouldNotLeak() {
        ReferenceQueue<InstrumentationActivity> referenceQueue = new ReferenceQueue<>();
        PhantomReference<InstrumentationActivity> reference;
        {
            InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
            // This installs a fullscreen callback, and is to ensure setting a fullscreen callback
            // doesn't leak.
            TestFullscreenCallback fullscreenCallback =
                    new TestFullscreenCallback(mActivityTestRule);
            mActivityTestRule.recreateActivity();
            boolean destroyed =
                    TestThreadUtils.runOnUiThreadBlockingNoException(() -> activity.isDestroyed());
            Assert.assertTrue(destroyed);

            reference = new PhantomReference<>(activity, referenceQueue);
        }

        Runtime.getRuntime().gc();
        CriteriaHelper.pollInstrumentationThread(() -> {
            Reference enqueuedReference = referenceQueue.poll();
            if (enqueuedReference == null) {
                Runtime.getRuntime().gc();
                throw new CriteriaNotSatisfiedException("No enqueued reference");
            }
            Assert.assertEquals(reference, enqueuedReference);
        });
    }

    @Test
    @SmallTest
    public void testRecreateInstance() {
        try {
            InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
            });
            mActivityTestRule.setRetainInstance(false);
            Fragment firstFragment = mActivityTestRule.getFragment();

            mActivityTestRule.recreateByRotatingToLandscape();
            boolean destroyed =
                    TestThreadUtils.runOnUiThreadBlockingNoException(() -> activity.isDestroyed());
            Assert.assertTrue(destroyed);

            Fragment secondFragment = mActivityTestRule.getFragment();
            Assert.assertNotSame(firstFragment, secondFragment);
        } finally {
            Activity activity = mActivityTestRule.getActivity();
            TestThreadUtils.runOnUiThreadBlocking(() -> {
                activity.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
            });
        }
    }

    @Test
    @SmallTest
    public void testSetRetainInstance() {
        ReferenceQueue<InstrumentationActivity> referenceQueue = new ReferenceQueue<>();
        PhantomReference<InstrumentationActivity> reference;
        {
            InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");

            mActivityTestRule.setRetainInstance(true);
            Fragment firstFragment = mActivityTestRule.getFragment();
            mActivityTestRule.recreateActivity();
            Fragment secondFragment = mActivityTestRule.getFragment();
            Assert.assertEquals(firstFragment, secondFragment);

            boolean destroyed =
                    TestThreadUtils.runOnUiThreadBlockingNoException(() -> activity.isDestroyed());
            Assert.assertTrue(destroyed);
            reference = new PhantomReference<>(activity, referenceQueue);
        }

        CriteriaHelper.pollInstrumentationThread(() -> {
            Reference enqueuedReference = referenceQueue.poll();
            if (enqueuedReference == null) {
                Runtime.getRuntime().gc();
                throw new CriteriaNotSatisfiedException("No enqueued reference");
            }
            Assert.assertEquals(reference, enqueuedReference);
        });
    }

    // Verifies recreating the Activity destroys the original Fragment when using ViewModel.
    @Test
    @SmallTest
    public void testRecreateActivityDestroysFragmentUseViewModel() throws Throwable {
        Bundle extras = new Bundle();
        extras.putBoolean(InstrumentationActivity.EXTRA_USE_VIEW_MODEL, true);
        mActivityTestRule.launchShellWithUrl("about:blank", extras);
        ReferenceQueue<Fragment> referenceQueue = new ReferenceQueue<>();
        PhantomReference<Fragment> reference =
                new PhantomReference<>(mActivityTestRule.getFragment(), referenceQueue);
        mActivityTestRule.recreateActivity();
        CriteriaHelper.pollInstrumentationThread(() -> {
            Reference enqueuedReference = referenceQueue.poll();
            if (enqueuedReference == null) {
                Runtime.getRuntime().gc();
                throw new CriteriaNotSatisfiedException("No enqueued reference");
            }
            Assert.assertEquals(reference, enqueuedReference);
        });
    }
}
