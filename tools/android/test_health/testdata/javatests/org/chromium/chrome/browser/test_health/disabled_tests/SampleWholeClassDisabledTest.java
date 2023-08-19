// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.test_health.disabled_tests;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DisabledTest;

/** A sample Java test with all test cases disabled by a class-level annotation. */
@SmallTest
@RunWith(BaseJUnit4ClassRunner.class)
@DisabledTest(message = "Disable whole test class")
public class SampleWholeClassDisabledTest {
    @Test
    public void testFalseIsTrue() {
        Assert.assertTrue(false);
    }

    @Test
    public void testTrueIsFalse() {
        // Disabled since true is never false.
        Assert.assertFalse(true);
    }
}
