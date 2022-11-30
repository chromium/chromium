// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.test_health.disabled_tests;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DisabledTest;

/** A sample Java test with disabled test cases. */
@SmallTest
@RunWith(BaseJUnit4ClassRunner.class)
public class SampleDisabledTest {
    @DisabledTest(message = "Disabled since false is never true.")
    @Test
    public void testFalseIsTrue() {
        Assert.assertTrue(false);
    }

    @DisabledTest
    @Test
    public void testTrueIsFalse() {
        // Disabled since true is never false.
        Assert.assertFalse(true);
    }
}
