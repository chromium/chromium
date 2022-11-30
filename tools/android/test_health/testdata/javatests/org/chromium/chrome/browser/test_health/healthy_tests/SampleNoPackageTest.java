// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;

/** A sample Java test in the default Java package. */
@SmallTest
@RunWith(BaseJUnit4ClassRunner.class)
public class SampleNoPackageTest {
    @Test
    public void testTrueIsTrue() {
        Assert.assertTrue(true);
    }
}
