// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.test_health.unhealthy_tests;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;

import java.util.Arrays;

/** A Java test with invalid syntax. */
@SmallTest
@RunWith(BaseJUnit4ClassRunner.class)
public class InvalidSyntaxTest {
    private static final String[][] STRING_ARRAY_2D =
            new String[][] {new String[] {"hello", "world"}, new String[] {"foo", "bar"}};

    @Test
    public void testMethodReferenceFromArrayType() {
        String[][] values;

        // The javalang Python module doesn't support method references for array types:
        // https://github.com/c2nes/javalang/blob/566963547575e93d305871d9cb26ce47ff1a036e/javalang/test/test_java_8_syntax.py#L198-L204
        values = Arrays.stream(STRING_ARRAY_2D).map(String[]::clone).toArray(String[][]::new);

        Assert.assertEquals(STRING_ARRAY_2D, values);
    }
}
