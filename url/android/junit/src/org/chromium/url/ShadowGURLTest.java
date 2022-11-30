// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.url;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests of {@link ShadowGURL}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowGURL.class})
public class ShadowGURLTest {
    /* package */ static void deepAssertEquals(GURL expected, GURL actual) {
        Assert.assertEquals(expected, actual);
        Assert.assertEquals(expected.getScheme(), actual.getScheme());
        Assert.assertEquals(expected.getUsername(), actual.getUsername());
        Assert.assertEquals(expected.getPassword(), actual.getPassword());
        Assert.assertEquals(expected.getHost(), actual.getHost());
        Assert.assertEquals(expected.getPort(), actual.getPort());
        Assert.assertEquals(expected.getPath(), actual.getPath());
        Assert.assertEquals(expected.getQuery(), actual.getQuery());
        Assert.assertEquals(expected.getRef(), actual.getRef());
    }

    @Test
    public void testComponents() {
        GURL url = new GURL(JUnitTestGURLs.SEARCH_URL);
        Assert.assertFalse(url.isEmpty());
        Assert.assertTrue(url.isValid());

        Assert.assertEquals(JUnitTestGURLs.SEARCH_URL, url.getSpec());
        Assert.assertEquals("https", url.getScheme());
        Assert.assertEquals("", url.getUsername());
        Assert.assertEquals("", url.getPassword());
        Assert.assertEquals("www.google.com", url.getHost());
        Assert.assertEquals("", url.getPort());
        Assert.assertEquals("/search", url.getPath());
        Assert.assertEquals("q=test", url.getQuery());
        Assert.assertEquals("", url.getRef());
    }

    @Test
    public void testEmpty() {
        GURL url = new GURL("");
        Assert.assertFalse(url.isValid());

        Assert.assertEquals("", url.getSpec());
        Assert.assertEquals("", url.getScheme());
        Assert.assertEquals("", url.getUsername());
        Assert.assertEquals("", url.getPassword());
        Assert.assertEquals("", url.getHost());
        Assert.assertEquals("", url.getPort());
        Assert.assertEquals("", url.getPath());
        Assert.assertEquals("", url.getQuery());
        Assert.assertEquals("", url.getRef());
    }

    @Test
    public void testSerialization() {
        GURL gurl = new GURL(JUnitTestGURLs.URL_1_WITH_PATH);
        GURL deserialized = GURL.deserialize(gurl.serialize());

        deepAssertEquals(deserialized, gurl);
    }
}
