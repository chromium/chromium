// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.url;

import static org.mockito.Matchers.any;
import static org.mockito.Mockito.doThrow;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Log;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;

import java.util.Map;

/**
 * Tests for JUnitTestGURLs.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class JUnitTestGURLsTest {
    private static final String TAG = "JUnitTestGURLs";

    @Mock
    GURL.Natives mGURLMocks;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    private RuntimeException getErrorForGURL(GURL gurl) {
        String serialized = gurl.serialize();
        Assert.assertEquals(-1, serialized.indexOf(","));
        serialized = serialized.replace(GURL.SERIALIZER_DELIMITER, ',');

        return new RuntimeException("Please update the serialization in JUnitTestGURLs.java for "
                + gurl.getPossiblyInvalidSpec() + " to: '" + serialized + "'");
    }

    @SmallTest
    @Test
    public void testGURLEquivalence() throws Throwable {
        doThrow(new RuntimeException("Deserialization required re-initialization."))
                .when(mGURLMocks)
                .init(any(), any());

        Throwable exception = null;
        for (Map.Entry<String, String> entry : JUnitTestGURLs.sGURLMap.entrySet()) {
            GURL gurl = new GURL(entry.getKey());
            try {
                GURLJni.TEST_HOOKS.setInstanceForTesting(mGURLMocks);
                GURL deserialized = JUnitTestGURLs.getGURL(entry.getKey());
                GURLJni.TEST_HOOKS.setInstanceForTesting(null);
                GURLJavaTest.deepAssertEquals(deserialized, gurl);
            } catch (Throwable e) {
                GURLJni.TEST_HOOKS.setInstanceForTesting(null);
                exception = getErrorForGURL(gurl);
                Log.e(TAG, "Error: ", exception);
            }
        }
        if (exception != null) throw exception;
    }
}
