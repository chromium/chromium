// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import android.content.Context;
import android.os.Build;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.MinAndroidSdkLevel;

/**
 * Tests for (@link WebViewCompatibilityHelper}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class WebViewCompatibilityHelperTest {
    @Test
    @SmallTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.N)
    public void testLibraryPaths() throws Exception {
        Context appContext = InstrumentationRegistry.getTargetContext();
        ClassLoader classLoader = WebViewCompatibilityHelper.initialize(appContext);
        String[] libraryPaths = WebViewCompatibilityHelper.getLibraryPaths(classLoader);
        for (String path : libraryPaths) {
            Assert.assertTrue(path.startsWith("/./"));
        }
    }
}
