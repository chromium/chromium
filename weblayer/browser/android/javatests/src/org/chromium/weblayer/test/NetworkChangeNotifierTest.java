// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.os.RemoteException;

import androidx.test.filters.SmallTest;

import org.json.JSONException;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

/** Tests that integration with NetworkStateNotifier works as expected. */
@RunWith(WebLayerJUnit4ClassRunner.class)
public final class NetworkChangeNotifierTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    @Test
    @SmallTest
    @Feature({"WebLayer"})
    public void testNetworkChangeNotifierAutoDetectRegistered()
            throws RemoteException, JSONException {
        // This creates an activity with WebLayer loaded, and causes WebLayer
        // code to start listening to changes in network connectivity.
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        TestWebLayer testWebLayer = TestWebLayer.getTestWebLayer(activity.getApplicationContext());
        Assert.assertTrue(testWebLayer.isNetworkChangeAutoDetectOn());
    }
}
