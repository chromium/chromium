// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.test;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.net.Uri;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.webengine.Tab;
import org.chromium.webengine.WebEngine;
import org.chromium.webengine.WebEngineParams;
import org.chromium.webengine.WebSandbox;

import java.util.Set;

/**
 * Tests the persistence state of WebEngine.
 */
@DoNotBatch(reason = "Tests need separate Activities and WebFragments")
@RunWith(WebEngineJUnit4ClassRunner.class)
public class StatePersistenceTest {
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

    private String getTestDataURL(String path) {
        return mServer.getURL("/weblayer/test/data/" + path);
    }

    @Test
    @SmallTest
    public void tabsPersistAcrossSessions() throws Exception {
        WebEngineParams params = (new WebEngineParams.Builder())
                                         .setPersistenceId("pid1234")
                                         .setProfileName("pn12345")
                                         .build();
        WebEngine webEngine = runOnUiThreadBlocking(
                () -> mActivityTestRule.getWebSandbox().createWebEngine(params))
                                      .get();
        Tab activeTab = webEngine.getTabManager().getActiveTab();
        String url = getTestDataURL("simple_page.html");
        mActivityTestRule.navigateAndWait(activeTab, url);
        // Shutdown the sandbox.
        runOnUiThreadBlocking(() -> {
            try {
                mActivityTestRule.getWebSandbox().shutdown();
            } catch (Exception e) {
                Assert.fail("Failed to shutdown sandbox");
            }
        });

        WebSandbox sandbox =
                runOnUiThreadBlocking(() -> WebSandbox.create(ContextUtils.getApplicationContext()))
                        .get();
        // Recreate a WebEngine with the same params.
        WebEngine webEngine2 = runOnUiThreadBlocking(() -> sandbox.createWebEngine(params)).get();
        Tab newActiveTab = webEngine2.getTabManager().getActiveTab();

        Assert.assertEquals(url, newActiveTab.getDisplayUri().toString());
        Assert.assertEquals(newActiveTab.getGuid(), activeTab.getGuid());

        Set<Tab> allTabs = webEngine2.getTabManager().getAllTabs();
        Assert.assertEquals(1, allTabs.size());
        Assert.assertEquals(newActiveTab, allTabs.iterator().next());
    }

    @Test
    @SmallTest
    public void incognitoModeNotInterferingWithPerisitenceState() throws Exception {
        String perisistenceId = "pid1234";
        String profileName = "pn12345";
        WebEngineParams params = (new WebEngineParams.Builder())
                                         .setPersistenceId(perisistenceId)
                                         .setProfileName(profileName)
                                         .build();
        // Create an initial WebEngine state and associate it with a persistence ID.
        WebEngine webEngine = runOnUiThreadBlocking(
                () -> mActivityTestRule.getWebSandbox().createWebEngine(params))
                                      .get();
        String url = getTestDataURL("simple_page.html");
        Tab activeTab = webEngine.getTabManager().getActiveTab();
        mActivityTestRule.navigateAndWait(activeTab, url);
        runOnUiThreadBlocking(() -> webEngine.close());

        // Recreate a WebEngine in incognito mode.
        WebEngineParams paramsIncognito = (new WebEngineParams.Builder())
                                                  .setPersistenceId(perisistenceId)
                                                  .setProfileName(profileName)
                                                  .setIsIncognito(true)
                                                  .build();
        WebEngine webEngineIncognito = runOnUiThreadBlocking(
                () -> mActivityTestRule.getWebSandbox().createWebEngine(paramsIncognito))
                                               .get();
        // Test that WebEngine did not recreate based on persistence ID.
        Tab incognitoTab = webEngineIncognito.getTabManager().getActiveTab();
        Assert.assertNotEquals(activeTab.getGuid(), incognitoTab.getGuid());
        Assert.assertEquals(Uri.EMPTY, incognitoTab.getDisplayUri());
        Assert.assertEquals(1, webEngineIncognito.getTabManager().getAllTabs().size());

        runOnUiThreadBlocking(() -> webEngineIncognito.close());

        // Recreate WebEngine with perisistence ID, and verify that state was not changed.
        WebEngine webEngine2 = runOnUiThreadBlocking(
                () -> mActivityTestRule.getWebSandbox().createWebEngine(params))
                                       .get();
        Assert.assertEquals(
                url, webEngine2.getTabManager().getActiveTab().getDisplayUri().toString());
        Assert.assertEquals(1, webEngine2.getTabManager().getAllTabs().size());
    }
}
