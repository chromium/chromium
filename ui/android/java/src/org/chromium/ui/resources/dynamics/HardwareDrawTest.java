// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.resources.dynamics;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.os.Build.VERSION_CODES;
import android.view.View;
import android.widget.LinearLayout;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;

/** On device unit test for {@link HardwareDraw}. */
@RunWith(BaseJUnit4ClassRunner.class)
@MinAndroidSdkLevel(VERSION_CODES.Q)
@Batch(Batch.UNIT_TESTS)
public class HardwareDrawTest extends BlankUiTestActivityTestCase {
    private View mView;
    private HardwareDraw mHardwareDraw;
    private List<Bitmap> mCapturedBitmaps;

    private CaptureObserver mCaptureObserver = new CaptureObserver() {
        @Override
        public void onCaptureStart(Canvas canvas, Rect dirtyRect) {}
        @Override
        public void onCaptureEnd() {}
    };

    Callback<Bitmap> mOnCapture = (Bitmap bitmap) -> {
        mCapturedBitmaps.add(bitmap);
    };

    private boolean startBitmapCapture() {
        AtomicBoolean atomicBoolean = new AtomicBoolean();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Rect dirtyRect = new Rect(0, 0, mView.getWidth(), mView.getHeight());
            atomicBoolean.set(mHardwareDraw.startBitmapCapture(
                    mView, dirtyRect, 1, mCaptureObserver, mOnCapture));
        });
        return atomicBoolean.get();
    }

    @Before
    public void setup() {
        mCapturedBitmaps = new ArrayList<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Activity activity = getActivity();
            mView = new LinearLayout(activity);
            activity.setContentView(mView);
            mHardwareDraw = new HardwareDraw();
        });
    }

    @Test
    @SmallTest
    public void testDropCachedBitmap_notInitialized() {
        // Verifies we do not NPE during #dropCachedBitmap, see https://crbug.com/1344654.
        TestThreadUtils.runOnUiThreadBlocking(() -> { mHardwareDraw.dropCachedBitmap(); });
    }

    @Test
    @MediumTest
    public void testStartBitmapCapture_raceCondition() throws Exception {
        // Verifies rapid captures does not get stuck, see https://crbug.com/1344612. This test is
        // inherently racy and it is likely it could let false negatives through.

        TestThreadUtils.runOnUiThreadBlocking(() -> { mHardwareDraw.onViewSizeChange(mView, 1); });

        int captureTakenCount = 0;
        // Setting this to over 256 will cause histograms default w/o native to blow up.
        for (int i = 0; i < 100; i++) {
            if (startBitmapCapture()) {
                captureTakenCount++;
            }
            // Pause on some of the iterations to give the various threads a chance to do things.
            // A captures goes from UI -> TaskRunner -> Handler -> TaskRunner -> UI. Each step takes
            // 3-6ms, the whole cycle takes ~20ms. Though some devices will be faster and slower.
            // The race we're targeting is between the last TaskRunner -> UI jump.
            if (i % 20 == 0) {
                Thread.sleep(20);
            } else if (i % 10 == 0) {
                Thread.sleep(5);
            } else if (i % 3 == 0) {
                Thread.sleep(1);
            }
        }

        final int finalExpectedCount = captureTakenCount;
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mCapturedBitmaps.size(), Matchers.equalTo(finalExpectedCount));
        });

        // With the above waits, a typical device will see ~10 captures. All devices should see at
        // least 2 captures 100% of the time.
        Assert.assertTrue(mCapturedBitmaps.size() >= 2);
    }
}