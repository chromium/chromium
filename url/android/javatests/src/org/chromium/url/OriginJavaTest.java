// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.url;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.mojo_base.mojom.UnguessableToken;

/**
 * Tests for {@link Origin}. Origin relies heavily on the native implementation, and the lion's
 * share of the logic is tested there. This test is primarily to make sure everything is plumbed
 * through correctly.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class OriginJavaTest {
    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
    }

    @SmallTest
    @Test
    public void testOriginEquivalence() {
        OriginJavaTestHelper.testOriginEquivalence();
    }

    @SmallTest
    @Test
    public void testCreateOpaqueOrigin() {
        Origin opaque = Origin.createOpaqueOrigin();
        Assert.assertTrue(opaque.isOpaque());
        Assert.assertEquals("", opaque.getScheme());
        Assert.assertEquals("", opaque.getHost());
        Assert.assertEquals(0, opaque.getPort());
    }

    @SmallTest
    @Test
    public void testNonOpaqueMojomConstructor() {
        String scheme = "http";
        String host = "host.name";
        short port = 42;
        org.chromium.url.internal.mojom.Origin mojom = new org.chromium.url.internal.mojom.Origin();
        mojom.scheme = scheme;
        mojom.host = host;
        mojom.port = port;
        Origin origin = new Origin(mojom);

        Assert.assertEquals(scheme, origin.getScheme());
        Assert.assertEquals(host, origin.getHost());
        Assert.assertEquals(port, origin.getPort());
        Assert.assertFalse(origin.isOpaque());
    }

    @SmallTest
    @Test
    public void testOpaqueMojomConstructor() {
        String scheme = "http";
        String host = "host.name";
        short port = 42;
        org.chromium.url.internal.mojom.Origin mojom = new org.chromium.url.internal.mojom.Origin();
        mojom.scheme = scheme;
        mojom.host = host;
        mojom.port = port;
        UnguessableToken token = new UnguessableToken();
        token.high = 3;
        token.low = 4;
        mojom.nonceIfOpaque = token;

        Origin origin = new Origin(mojom);

        Assert.assertEquals("", origin.getScheme());
        Assert.assertEquals("", origin.getHost());
        Assert.assertEquals(0, origin.getPort());
        Assert.assertTrue(origin.isOpaque());
    }

    @SmallTest
    @Test
    public void testCreateFromGURL() {
        GURL gurl = new GURL("https://host.name:61234/path");
        Origin opaque = Origin.create(gurl);
        Assert.assertFalse(opaque.isOpaque());
        Assert.assertEquals("https", opaque.getScheme());
        Assert.assertEquals("host.name", opaque.getHost());
        Assert.assertEquals(61234, opaque.getPort());
    }
}
