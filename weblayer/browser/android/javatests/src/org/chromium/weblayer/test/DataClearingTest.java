// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;
import static org.chromium.weblayer.BrowsingDataType.CACHE;
import static org.chromium.weblayer.BrowsingDataType.COOKIES_AND_SITE_DATA;

import androidx.fragment.app.FragmentManager;
import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.weblayer.Profile;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Example test that just starts the weblayer shell.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class DataClearingTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    @Test
    @SmallTest
    public void clearDataWithPersistedProfile_TriggersCallback() {
        checkTriggersCallbackOnClearData(new int[] {COOKIES_AND_SITE_DATA}, "Profile");
    }

    @Test
    @SmallTest
    public void clearDataWithInMemoryProfile_TriggersCallback() {
        checkTriggersCallbackOnClearData(new int[] {COOKIES_AND_SITE_DATA}, null);
    }

    @Test
    @SmallTest
    public void clearCacheWithPersistedProfile_TriggersCallback() {
        checkTriggersCallbackOnClearData(new int[] {CACHE}, "Profile");
    }

    @Test
    @SmallTest
    public void clearCacheWithInMemoryProfile_TriggersCallback() {
        checkTriggersCallbackOnClearData(new int[] {CACHE}, null);
    }

    @Test
    @SmallTest
    public void clearMultipleTypes_TriggersCallback() {
        checkTriggersCallbackOnClearData(new int[] {COOKIES_AND_SITE_DATA, CACHE}, "Profile");
    }

    @Test
    @SmallTest
    public void clearUnknownType_TriggersCallback() {
        // This is a forward compatibility test: the older versions of Chrome that don't yet
        // implement clearing some data type should just ignore it and call the callback.
        checkTriggersCallbackOnClearData(new int[] {9999}, "Profile");
    }

    @Test
    @SmallTest
    public void twoSuccesiveRequestsTriggerCallbacks() {
        InstrumentationActivity activity = mActivityTestRule.launchWithProfile("profile");

        BoundedCountDownLatch latch = new BoundedCountDownLatch(2);
        runOnUiThreadBlocking(() -> {
            Profile profile = activity.getBrowser().getProfile();
            profile.clearBrowsingData(new int[] {COOKIES_AND_SITE_DATA}, latch::countDown);
            profile.clearBrowsingData(new int[] {CACHE}, latch::countDown);
        });
        latch.timedAwait();
    }

    @Test
    @SmallTest
    public void clearingAgainAfterClearFinished_TriggersCallback() {
        InstrumentationActivity activity = mActivityTestRule.launchWithProfile("profile");

        BoundedCountDownLatch latch = new BoundedCountDownLatch(1);
        runOnUiThreadBlocking(() -> {
            Profile profile = activity.getBrowser().getProfile();
            profile.clearBrowsingData(new int[] {COOKIES_AND_SITE_DATA},
                    () -> { profile.clearBrowsingData(new int[] {CACHE}, latch::countDown); });
        });
        latch.timedAwait();
    }

    @Test
    @SmallTest
    public void destroyingProfileDuringDataClear_DoesntCrash() {
        InstrumentationActivity activity = mActivityTestRule.launchWithProfile("profile");

        BoundedCountDownLatch latch = new BoundedCountDownLatch(1);
        runOnUiThreadBlocking(() -> {
            Profile profile = activity.getBrowser().getProfile();
            profile.clearBrowsingData(new int[] {COOKIES_AND_SITE_DATA}, () -> {});

            // We need to remove the fragment before calling Profile#destroy().
            FragmentManager fm = activity.getSupportFragmentManager();
            fm.beginTransaction().remove(fm.getFragments().get(0)).commitNow();

            profile.destroy();
            latch.countDown();
        });
        latch.timedAwait();
    }

    private void checkTriggersCallbackOnClearData(int[] dataTypes, String profileName) {
        InstrumentationActivity activity = mActivityTestRule.launchWithProfile(profileName);
        BoundedCountDownLatch latch = new BoundedCountDownLatch(1);
        runOnUiThreadBlocking(() -> {
            activity.getBrowser().getProfile().clearBrowsingData(dataTypes, latch::countDown);
        });
        latch.timedAwait();
    }
}
