// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.test_health.flaky_tests;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;

import java.util.Random;

/** A sample Java test with flaky test cases. */
@SmallTest
@RunWith(BaseJUnit4ClassRunner.class)
public class SampleFlakyTest {
    @FlakyTest(message = "Flaky since value is sometimes false.")
    @Test
    public void testFalseIsTrue() {
        Random random = new Random();

        boolean value = random.nextBoolean();

        Assert.assertTrue(value);
    }

    @FlakyTest
    @Test
    public void testTrueIsFalse() {
        // Flaky since value is sometimes true.
        Random random = new Random();

        boolean value = random.nextBoolean();

        Assert.assertFalse(value);
    }
}
