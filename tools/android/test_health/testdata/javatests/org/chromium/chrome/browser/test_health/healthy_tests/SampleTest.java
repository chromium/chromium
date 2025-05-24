// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.test_health.healthy_tests;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.test.transit.MyStation;

/** A sample Java test. */
@SmallTest
@RunWith(BaseJUnit4ClassRunner.class)
public class SampleTest {
    @Test
    public void testTrueIsTrue() {
        MyStation myStation = new MyStation();
        Assert.assertTrue(true);
    }
}
