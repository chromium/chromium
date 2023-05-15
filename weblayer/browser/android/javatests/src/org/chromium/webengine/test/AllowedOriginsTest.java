// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.test;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.webengine.Navigation;
import org.chromium.webengine.Tab;
import org.chromium.webengine.WebEngine;
import org.chromium.webengine.WebEngineParams;
import org.chromium.webengine.WebSandbox;

import java.util.ArrayList;

/**
 * Tests allowed origins. This can be provided as a WebEngine parameter.
 */
@DoNotBatch(reason = "Tests need separate Activities and WebFragments")
@RunWith(WebEngineJUnit4ClassRunner.class)
public class AllowedOriginsTest {
    private static final int PORT = 3000;

    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private EmbeddedTestServer mServer;

    WebSandbox mSandbox;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchShell();
        // The origin needs to specify the port so instead of auto-assigning one, we need to choose
        // an explicit port in our tests.
        mTestServerRule.setServerPort(PORT);
        mServer = mTestServerRule.getServer();
        mSandbox = mActivityTestRule.getWebSandbox();
    }

    @After
    public void shutdown() throws Exception {
        if (mSandbox != null) {
            runOnUiThreadBlocking(() -> mSandbox.shutdown());
        }
        mActivityTestRule.finish();
    }

    @Test
    @SmallTest
    public void testLoadsUrls_whenAllowlistNotProvided() throws Exception {
        tryNavigateUrls(null);
    }

    @Test
    @SmallTest
    public void testLoadsUrls_whenOriginsAllowlisted() throws Exception {
        ArrayList<String> allowList = new ArrayList<>();
        allowList.add("http://localhost:" + PORT);
        allowList.add("http://127.0.0.1:" + PORT);

        tryNavigateUrls(allowList);
    }

    @Test()
    @SmallTest
    public void testDoNotLoadUrl_whenOriginNotAllowlisted() throws Exception {
        ArrayList<String> allowList = new ArrayList<>();
        allowList.add("http://localhost:" + PORT);
        // Not allowing 127.0.0.1

        try {
            tryNavigateUrls(allowList);
            Assert.fail("Should have thrown exception");
        } catch (InstrumentationActivityTestRule.NavigationFailureException e) {
            Navigation failedNavigation = e.getNavigation();
            // Status code is 0 when a navigation hasn't completed
            Assert.assertEquals(failedNavigation.getStatusCode(), 0);
            // We can verify this failed for 127.0.0.1
            Assert.assertEquals(failedNavigation.getUri().toString(),
                    getTestDataURL("127.0.0.1", "simple_page.html"));
        }
    }

    @Test(expected = InstrumentationActivityTestRule.NavigationFailureException.class)
    @SmallTest()
    public void testDoNotLoadUrl_whenPortNotAllowlisted() throws Exception {
        ArrayList<String> allowList = new ArrayList<>();
        allowList.add("http://localhost:" + PORT);
        // This will fail because this is using a port that wasn't allowlisted
        allowList.add("http://127.0.0.1:" + (PORT + 1));

        tryNavigateUrls(allowList);
    }

    @Test(expected = InstrumentationActivityTestRule.NavigationFailureException.class)
    @SmallTest()
    public void testDoNotLoadUrl_whenSchemeNotAllowlisted() throws Exception {
        ArrayList<String> allowList = new ArrayList<>();
        allowList.add("http://localhost:" + PORT);
        // Our test server uses http so this will fail.
        allowList.add("https://127.0.0.1:" + PORT);

        tryNavigateUrls(allowList);
    }

    private void tryNavigateUrls(ArrayList<String> allowList) throws Exception {
        WebEngine webEngine = createWebEngine(allowList);

        String url1 = getTestDataURL("localhost", "simple_page.html");
        String url2 = getTestDataURL("127.0.0.1", "simple_page.html");

        Tab activeTab = webEngine.getTabManager().getActiveTab();
        mActivityTestRule.navigateAndWait(activeTab, url1);
        mActivityTestRule.navigateAndWait(activeTab, url2);
    }

    private WebEngine createWebEngine(ArrayList<String> allowList) throws Exception {
        WebEngineParams.Builder builder =
                (new WebEngineParams.Builder()).setProfileName("DefaultProfile");

        if (allowList != null) {
            builder.setAllowedOrigins(allowList);
        }

        WebEngineParams params = builder.build();

        return runOnUiThreadBlocking(() -> mSandbox.createWebEngine(params)).get();
    }

    private String getTestDataURL(String host, String path) {
        return mServer.getURLWithHostName(host, "/weblayer/test/data/" + path);
    }
}
