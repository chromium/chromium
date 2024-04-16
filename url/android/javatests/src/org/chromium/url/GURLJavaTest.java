// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.url;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doThrow;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.build.BuildConfig;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

import java.net.URISyntaxException;

/**
 * Tests for {@link GURL}. GURL relies heavily on the native implementation, and the lion's share of
 * the logic is tested there. This test is primarily to make sure everything is plumbed through
 * correctly.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class GURLJavaTest {
    @Mock GURL.Natives mGURLMocks;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
        GURLJavaTestHelper.nativeInitializeICU();
    }

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

    private String prependLengthToSerialization(String serialization) {
        return Integer.toString(serialization.length()) + GURL.SERIALIZER_DELIMITER + serialization;
    }

    @SmallTest
    @Test
    public void testGURLEquivalence() {
        GURLJavaTestHelper.nativeTestGURLEquivalence();
    }

    // Equivalent of GURLTest.Components
    @SmallTest
    @Test
    @SuppressWarnings(value = "AuthLeak")
    public void testComponents() {
        GURL empty = new GURL("");
        Assert.assertTrue(empty.isEmpty());
        Assert.assertFalse(empty.isValid());

        GURL url = new GURL("http://user:pass@google.com:99/foo;bar?q=a#ref");
        Assert.assertFalse(url.isEmpty());
        Assert.assertTrue(url.isValid());
        Assert.assertTrue(url.getScheme().equals("http"));

        Assert.assertEquals("http://user:pass@google.com:99/foo;bar?q=a#ref", url.getSpec());

        Assert.assertEquals("http", url.getScheme());
        Assert.assertEquals("user", url.getUsername());
        Assert.assertEquals("pass", url.getPassword());
        Assert.assertEquals("google.com", url.getHost());
        Assert.assertEquals("99", url.getPort());
        Assert.assertEquals("/foo;bar", url.getPath());
        Assert.assertEquals("q=a", url.getQuery());
        Assert.assertEquals("ref", url.getRef());

        // Test parsing userinfo with special characters.
        GURL urlSpecialPass = new GURL("http://user:%40!$&'()*+,;=:@google.com:12345");
        Assert.assertTrue(urlSpecialPass.isValid());
        // GURL canonicalizes some delimiters.
        Assert.assertEquals("%40!$&%27()*+,%3B%3D%3A", urlSpecialPass.getPassword());
        Assert.assertEquals("google.com", urlSpecialPass.getHost());
        Assert.assertEquals("12345", urlSpecialPass.getPort());
    }

    // Equivalent of GURLTest.Empty
    @SmallTest
    @Test
    public void testEmpty() {
        GURLJni.TEST_HOOKS.setInstanceForTesting(mGURLMocks);
        doThrow(new RuntimeException("Should not need to parse empty URL"))
                .when(mGURLMocks)
                .init(any(), any());
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
        GURLJni.TEST_HOOKS.setInstanceForTesting(null);
    }

    // Test that GURL and URI return the correct Origin.
    @SmallTest
    @Test
    @SuppressWarnings(value = "AuthLeak")
    public void testOrigin() throws URISyntaxException {
        final String kExpectedOrigin1 = "http://google.com:21/";
        final String kExpectedOrigin2 = "";
        GURL url1 = new GURL("filesystem:http://user:pass@google.com:21/blah#baz");
        GURL url2 = new GURL("javascript:window.alert(\"hello,world\");");
        URI uri = new URI("filesystem:http://user:pass@google.com:21/blah#baz");

        Assert.assertEquals(kExpectedOrigin1, url1.getOrigin().getSpec());
        Assert.assertEquals(kExpectedOrigin2, url2.getOrigin().getSpec());
        URI origin = uri.getOrigin();
        Assert.assertEquals(kExpectedOrigin1, origin.getSpec());
    }

    @SmallTest
    @Test
    public void testWideInput() throws URISyntaxException {
        final String kExpectedSpec = "http://xn--1xa.com/";

        GURL url = new GURL("http://\u03C0.com");
        Assert.assertEquals(kExpectedSpec, url.getSpec());
        Assert.assertEquals("http", url.getScheme());
        Assert.assertEquals("", url.getUsername());
        Assert.assertEquals("", url.getPassword());
        Assert.assertEquals("xn--1xa.com", url.getHost());
        Assert.assertEquals("", url.getPort());
        Assert.assertEquals("/", url.getPath());
        Assert.assertEquals("", url.getQuery());
        Assert.assertEquals("", url.getRef());
    }

    @SmallTest
    @Test
    @SuppressWarnings(value = "AuthLeak")
    public void testSerialization() {
        GURL cases[] = {
            // Common Standard URLs.
            new GURL("https://www.google.com"),
            new GURL("https://www.google.com/"),
            new GURL("https://www.google.com/maps.htm"),
            new GURL("https://www.google.com/maps/"),
            new GURL("https://www.google.com/index.html"),
            new GURL("https://www.google.com/index.html?q=maps"),
            new GURL("https://www.google.com/index.html#maps/"),
            new GURL("https://foo:bar@www.google.com/maps.htm"),
            new GURL("https://www.google.com/maps/au/index.html"),
            new GURL("https://www.google.com/maps/au/north"),
            new GURL("https://www.google.com/maps/au/north/"),
            new GURL("https://www.google.com/maps/au/index.html?q=maps#fragment/"),
            new GURL("http://www.google.com:8000/maps/au/index.html?q=maps#fragment/"),
            new GURL("https://www.google.com/maps/au/north/?q=maps#fragment"),
            new GURL("https://www.google.com/maps/au/north?q=maps#fragment"),
            // Less common standard URLs.
            new GURL("filesystem:http://www.google.com/temporary/bar.html?baz=22"),
            new GURL("file:///temporary/bar.html?baz=22"),
            new GURL("ftp://foo/test/index.html"),
            new GURL("gopher://foo/test/index.html"),
            new GURL("ws://foo/test/index.html"),
            // Non-standard,
            new GURL("chrome://foo/bar.html"),
            new GURL("httpa://foo/test/index.html"),
            new GURL("blob:https://foo.bar/test/index.html"),
            new GURL("about:blank"),
            new GURL("data:foobar"),
            new GURL("scheme:opaque_data"),
            // Invalid URLs.
            new GURL("foobar"),
            // URLs containing the delimiter
            new GURL("https://www.google.ca/" + GURL.SERIALIZER_DELIMITER + ",foo"),
            new GURL("https://www.foo" + GURL.SERIALIZER_DELIMITER + "bar.com"),
        };

        GURLJni.TEST_HOOKS.setInstanceForTesting(mGURLMocks);
        doThrow(
                        new RuntimeException(
                                "Should not re-initialize for deserialization when the "
                                        + "version hasn't changed."))
                .when(mGURLMocks)
                .init(any(), any());
        for (GURL url : cases) {
            GURL out = GURL.deserialize(url.serialize());
            deepAssertEquals(url, out);
        }
        GURLJni.TEST_HOOKS.setInstanceForTesting(null);
    }

    /**
     * Tests that we re-parse the URL from the spec, which must always be the last token in the
     * serialization, if the serialization version differs.
     */
    @SmallTest
    @Test
    public void testSerializationWithVersionSkew() {
        GURL url = new GURL("https://www.google.com");
        String serialization =
                (GURL.SERIALIZER_VERSION + 1)
                        + ",0,0,0,0,foo,https://url.bad,blah,0,"
                                .replace(',', GURL.SERIALIZER_DELIMITER)
                        + url.getSpec();
        serialization = prependLengthToSerialization(serialization);
        GURL out = GURL.deserialize(serialization);
        deepAssertEquals(url, out);
    }

    /** Tests that fields that aren't visible to java code are correctly serialized. */
    @SmallTest
    @Test
    public void testSerializationOfPrivateFields() {
        String serialization =
                GURL.SERIALIZER_VERSION
                        + ",true,"
                        // Outer Parsed.
                        + "1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,false,true,"
                        // Inner Parsed.
                        + "17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,true,false,"
                        + "chrome://foo/bar.html";
        serialization = serialization.replace(',', GURL.SERIALIZER_DELIMITER);
        serialization = prependLengthToSerialization(serialization);
        GURL url = GURL.deserialize(serialization);
        Assert.assertEquals(url.serialize(), serialization);
    }

    /** Tests serialized GURL truncated by storage. */
    @SmallTest
    @Test
    public void testTruncatedDeserialization() {
        String serialization = "123,1,true,1,2,3,4,5,6,7,8,9,10";
        serialization = serialization.replace(',', GURL.SERIALIZER_DELIMITER);
        GURL url = GURL.deserialize(serialization);
        Assert.assertEquals(url, GURL.emptyGURL());
    }

    /** Tests serialized GURL truncated by storage. */
    @SmallTest
    @Test
    public void testCorruptedSerializations() {
        String serialization = new GURL("https://www.google.ca").serialize();
        // Replace the scheme length (5) with an extra delimiter.
        String corruptedParsed = serialization.replace('5', GURL.SERIALIZER_DELIMITER);
        GURL url = GURL.deserialize(corruptedParsed);
        Assert.assertEquals(GURL.emptyGURL(), url);

        String corruptedVersion =
                serialization.replaceFirst(Integer.toString(GURL.SERIALIZER_VERSION), "x");
        url = GURL.deserialize(corruptedVersion);
        Assert.assertEquals(GURL.emptyGURL(), url);
    }

    // Test that domainIs is hooked up correctly.
    @SmallTest
    @Test
    public void testDomainIs() {
        GURL url1 = new GURL("https://www.google.com");
        GURL url2 = new GURL("https://www.notgoogle.com");

        Assert.assertTrue(url1.domainIs("com"));
        Assert.assertTrue(url2.domainIs("com"));
        Assert.assertTrue(url1.domainIs("google.com"));
        Assert.assertFalse(url2.domainIs("google.com"));

        Assert.assertTrue(url1.domainIs("www.google.com"));
        Assert.assertFalse(url1.domainIs("images.google.com"));
    }

    // Test that replaceComponents is hooked up correctly.
    @SmallTest
    @Test
    @SuppressWarnings(value = "AuthLeak")
    public void testReplaceComponents() {
        GURL url = new GURL("http://user:pass@google.com:99/foo;bar?q=a#ref");

        GURL unchanged = url.replaceComponents(null, false, null, false);
        Assert.assertEquals("user", unchanged.getUsername());
        Assert.assertEquals("pass", unchanged.getPassword());

        GURL cleared = url.replaceComponents(null, true, null, true);
        Assert.assertTrue(cleared.getUsername().isEmpty());
        Assert.assertTrue(cleared.getPassword().isEmpty());

        GURL changed = url.replaceComponents("newusername", false, "newpassword", false);
        Assert.assertEquals("newusername", changed.getUsername());
        Assert.assertEquals("newpassword", changed.getPassword());
    }

    // Tests Mojom conversion.
    @SmallTest
    @Test
    public void testMojomConvertion() {
        // Valid:
        Assert.assertEquals(
                "https://www.google.com/", new GURL("https://www.google.com/").toMojom().url);

        // Null:
        Assert.assertEquals("", new GURL(null).toMojom().url);

        // Empty:
        Assert.assertEquals("", new GURL("").toMojom().url);

        // Invalid:
        Assert.assertEquals("", new GURL(new String(new byte[] {1, 1, 1})).toMojom().url);

        // Too long.
        Assert.assertEquals(
                "",
                new GURL("https://www.google.com/".concat("a".repeat(2 * 1024 * 1024)))
                        .toMojom()
                        .url);
    }

    /**
     * Verifies that GURL can be used in assertEquals() statements and similar assertion statements.
     */
    @SmallTest
    @Test
    public void testSupportsAssertEqualsComparison() {
        Assert.assertEquals(
                "GURLs created from the same spec should be equal",
                new GURL("https://example.test/"),
                new GURL("https://example.test/"));
        Assert.assertNotEquals(
                "GURLs with different paths are not equal",
                new GURL("https://example.test/"),
                new GURL("https://example.test/this/has/a/path.html"));
        Assert.assertNotEquals(
                "GURLs with different domains are not equal",
                new GURL("https://example.test/"),
                new GURL("https://different.test/"));
        Assert.assertNotEquals(
                "GURLs with different schemes are not equal",
                new GURL("https://example.test/"),
                new GURL("http://example.test/"));

        // When an assertion does fail, make sure that the failure message is human readable.
        Throwable exception =
                Assert.assertThrows(
                        AssertionError.class,
                        () -> {
                            Assert.assertEquals(
                                    new GURL("https://example.test/"),
                                    new GURL("https://different.test/"));
                        });
        if (BuildConfig.ENABLE_ASSERTS) {
            String expectedMessage =
                    "expected:<GURL(https://example.test/)> but"
                            + " was:<GURL(https://different.test/)>";
            Assert.assertEquals(
                    "Assertion message was not what we expected.",
                    expectedMessage,
                    exception.getMessage());
        }
    }
}
