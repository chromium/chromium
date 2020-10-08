// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.os.RemoteException;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tab tests that need to use WebLayerPrivate.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class TabPrivateTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private TestWebLayer getTestWebLayer() {
        return TestWebLayer.getTestWebLayer(mActivityTestRule.getContextForWebLayer());
    }

    @Test
    @SmallTest
    public void testCreateTabWithAccessibilityEnabledCrashTest() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            try {
                getTestWebLayer().setAccessibilityEnabled(true);
            } catch (RemoteException e) {
                Assert.fail("Unable to enable accessibility");
            }
            activity.getBrowser().createTab();
        });
    }
}
