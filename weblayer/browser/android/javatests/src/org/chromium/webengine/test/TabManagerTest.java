// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.test;

import static android.content.res.Configuration.ORIENTATION_LANDSCAPE;

import static org.hamcrest.CoreMatchers.is;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.content.pm.ActivityInfo;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.webengine.FragmentParams;
import org.chromium.webengine.Tab;
import org.chromium.webengine.TabListObserver;
import org.chromium.webengine.TabManager;
import org.chromium.webengine.WebEngine;
import org.chromium.webengine.WebSandbox;

import java.util.Set;
import java.util.concurrent.CountDownLatch;

/**
 * Tests various aspects of interacting with Tabs.
 */
@DoNotBatch(reason = "Tests need separate Activities and WebFragments")
@RunWith(WebEngineJUnit4ClassRunner.class)
public class TabManagerTest {
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
    public void tabGetsAddedAndActivatedOnStartup() throws Exception {
        WebEngine webEngine = runOnUiThreadBlocking(() -> mSandbox.createWebEngine()).get();

        Tab activeTab = webEngine.getTabManager().getActiveTab();
        Assert.assertNotNull(activeTab);

        Set<Tab> allTabs = webEngine.getTabManager().getAllTabs();
        Assert.assertEquals(1, allTabs.size());

        Assert.assertTrue(allTabs.contains(activeTab));
    }

    @Test
    @SmallTest
    public void tabsGetAddedToRegistry() throws Exception {
        WebEngine webEngine = runOnUiThreadBlocking(() -> mSandbox.createWebEngine()).get();

        Set<Tab> initialTabs = webEngine.getTabManager().getAllTabs();
        Assert.assertEquals(1, initialTabs.size());

        Tab addedTab = runOnUiThreadBlocking(() -> webEngine.getTabManager().createTab()).get();
        Set<Tab> twoTabs = webEngine.getTabManager().getAllTabs();
        Assert.assertEquals(2, twoTabs.size());
        Assert.assertTrue(twoTabs.contains(addedTab));
    }

    @Test
    @SmallTest
    public void tabsPersistAcrossRotations() throws Exception {
        String url = getTestDataURL("simple_page.html");
        WebEngine webEngine = mActivityTestRule.createWebEngineAttachThenNavigateAndWait(url);

        // Rotate device.
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    mActivityTestRule.getActivity().getResources().getConfiguration().orientation,
                    is(ORIENTATION_LANDSCAPE));
        });
        Tab activeTab = webEngine.getTabManager().getActiveTab();
        Assert.assertEquals(url, activeTab.getDisplayUri().toString());
    }

    @Test
    @SmallTest
    public void tabsPersistAcrossSessions() throws Exception {
        FragmentParams params = (new FragmentParams.Builder())
                                        .setPersistenceId("pid1234")
                                        .setProfileName("pn12345")
                                        .build();
        WebEngine webEngine = runOnUiThreadBlocking(
                () -> mActivityTestRule.getWebSandbox().createWebEngine(params))
                                      .get();
        runOnUiThreadBlocking(() -> mActivityTestRule.attachFragment(webEngine.getFragment()));
        Tab activeTab = webEngine.getTabManager().getActiveTab();
        String url = getTestDataURL("simple_page.html");
        mActivityTestRule.navigateAndWait(activeTab, url);
        // Shutdown the sandbox.
        runOnUiThreadBlocking(() -> mActivityTestRule.detachFragment(webEngine.getFragment()));
        runOnUiThreadBlocking(() -> {
            try {
                mActivityTestRule.getWebSandbox().shutdown();
            } catch (Exception e) {
                Assert.fail("Failed to shutdown sandbox");
            }
        });

        // Give the sandbox some time to shutdown.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        WebSandbox sandbox =
                runOnUiThreadBlocking(() -> WebSandbox.create(ContextUtils.getApplicationContext()))
                        .get();
        // Recreate a fragment with the same params.
        WebEngine webEngine2 = runOnUiThreadBlocking(() -> sandbox.createWebEngine(params)).get();
        runOnUiThreadBlocking(() -> mActivityTestRule.attachFragment(webEngine2.getFragment()));
        Tab newActiveTab = webEngine2.getTabManager().getActiveTab();
        Set<Tab> allTabs = webEngine2.getTabManager().getAllTabs();
        Assert.assertEquals(1, allTabs.size());
        Assert.assertTrue(allTabs.contains(newActiveTab));
        // TODO(crbug.com/1398388): Set url for tab after restoring from persistence ID.
        // Assert.assertEquals(newActiveTab.getDisplayUri().toString(), url);
        Assert.assertEquals(activeTab.getGuid(), newActiveTab.getGuid());
    }

    private static final class TabHolder {
        private Tab mAddedTab;
        private Tab mActiveTab;
        private Tab mRemovedTab;
    }

    @Test
    @SmallTest
    public void newTabCanBeActivatedAndRemoved() throws Exception {
        // One count for the initial tab created and one for the tab we programmatically create.
        CountDownLatch activeLatch = new CountDownLatch(1);
        CountDownLatch removeLatch = new CountDownLatch(1);

        final TabHolder holder = new TabHolder();
        WebEngine webEngine = runOnUiThreadBlocking(() -> mSandbox.createWebEngine()).get();
        runOnUiThreadBlocking(
                () -> webEngine.getTabManager().registerTabListObserver(new TabListObserver() {
                    @Override
                    public void onTabAdded(Tab tab) {
                        holder.mAddedTab = tab;
                    }

                    @Override
                    public void onActiveTabChanged(Tab tab) {
                        holder.mActiveTab = tab;
                        activeLatch.countDown();
                    }

                    @Override
                    public void onTabRemoved(Tab tab) {
                        holder.mRemovedTab = tab;
                        removeLatch.countDown();
                    }
                }));
        runOnUiThreadBlocking(() -> mActivityTestRule.attachFragment(webEngine.getFragment()));
        TabManager tabManager = webEngine.getTabManager();
        Tab newTab = runOnUiThreadBlocking(() -> tabManager.createTab()).get();
        Assert.assertEquals(newTab, holder.mAddedTab);

        runOnUiThreadBlocking(() -> newTab.setActive());
        activeLatch.await();
        Assert.assertEquals(newTab, holder.mActiveTab);

        runOnUiThreadBlocking(() -> newTab.close());
        removeLatch.await();
        Assert.assertEquals(newTab, holder.mRemovedTab);

        Assert.assertNull(tabManager.getActiveTab());
    }
}
