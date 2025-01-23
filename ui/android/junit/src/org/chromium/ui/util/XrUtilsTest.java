// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import static org.junit.Assert.assertNotNull;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link XrUtils} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class XrUtilsTest {

    @Test
    public void getInstanceTest_notNull() {
        // Verify test the instance is created.
        assertNotNull("XrUtils instance is missing.", XrUtils.getInstance());
    }
}
