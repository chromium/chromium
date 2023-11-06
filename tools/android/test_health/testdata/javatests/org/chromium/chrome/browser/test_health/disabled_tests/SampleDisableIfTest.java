// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.test_health.disabled_tests;

import android.os.Build.VERSION_CODES;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.DisableIf;

/** A sample Java test with conditionally disabled test cases. */
@SmallTest
@RunWith(BaseJUnit4ClassRunner.class)
public class SampleDisableIfTest {
    @DisableIf.Build(supported_abis_includes = "foo")
    @Test
    public void testFalseIsTrue() {
        Assert.assertTrue(false);
    }

    @DisableIf.Build(
            sdk_is_less_than = VERSION_CODES.BASE,
            message = "Disabled since true is never false.")
    @Test
    public void testTrueIsFalse() {
        Assert.assertFalse(true);
    }
}
