// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for the WebLayer class.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class WebLayerTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    @Test
    @SmallTest
    public void getUserAgentString() {
        final String userAgent = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> { return mActivityTestRule.getWebLayer().getUserAgentString(); });
        Assert.assertNotNull(userAgent);
        Assert.assertFalse(userAgent.isEmpty());
    }
}
