// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.test_health.unhealthy_tests;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;

/** A sample Java test. */
@SmallTest
@RunWith(BaseJUnit4ClassRunner.class)
public class SampleTest {
    @Test
    public void testTrueIsTrue() {
        Assert.assertTrue(true);
    }

    @Test
    public void testFalseIsFalse() {
        Assert.assertFalse(false);
    }

    @DisabledTest
    @Test
    public void testDisabledTest() {
        Assert.assertFalse(true);
    }

    @DisableIf.Build(supported_abis_includes = "foo")
    @Test
    public void testDisableIfTest() {
        Assert.assertTrue(false);
    }
}
