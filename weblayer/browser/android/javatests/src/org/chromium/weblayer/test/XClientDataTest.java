// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * WebLayer tests that need to use WebLayerPrivate.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
@CommandLineFlags.Add({"force-variation-ids=4,10,34"})
public class XClientDataTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    @MinWebLayerVersion(101)
    @Test
    @SmallTest
    public void getXClientDataHeader() {
        final String header = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> { return mActivityTestRule.getWebLayer().getXClientDataHeader(); });
        Assert.assertNotNull(header);
        Assert.assertFalse(header.isEmpty());
    }
}
