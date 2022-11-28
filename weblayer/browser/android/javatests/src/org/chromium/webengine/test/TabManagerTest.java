// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.test;

import static android.content.res.Configuration.ORIENTATION_LANDSCAPE;

import static org.hamcrest.CoreMatchers.is;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.content.pm.ActivityInfo;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;

import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.webengine.FragmentParams;
import org.chromium.webengine.Tab;
import org.chromium.webengine.TabListObserver;
import org.chromium.webengine.TabManager;
import org.chromium.webengine.WebFragment;
import org.chromium.webengine.WebSandbox;

import java.util.concurrent.CountDownLatch;

/**
 * Tests various aspects of interacting with Tabs.
 */
@Batch(Batch.PER_CLASS)
@RunWith(WebEngineJUnit4ClassRunner.class)
public class TabManagerTest {
    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private EmbeddedTestServer mServer;
    private WebFragment mFragment;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchShell();
        mServer = mTestServerRule.getServer();
        WebSandbox sandbox = mActivityTestRule.getWebSandbox();
        mFragment = runOnUiThreadBlocking(() -> sandbox.createFragment());
    }

    private String getTestDataURL(String path) {
        return mServer.getURL("/weblayer/test/data/" + path);
    }

    @Test
    @SmallTest
    public void tabGetsAddedAndActivatedOnStartup() throws Exception {
        CountDownLatch tabAddedLatch = new CountDownLatch(1);
        CountDownLatch tabActivatedLatch = new CountDownLatch(1);

        runOnUiThreadBlocking(() -> mFragment.registerTabListObserver(new TabListObserver() {
            @Nullable
            private Tab mAddedTab;

            @Override
            public void onTabAdded(Tab tab) {
                Assert.assertEquals(tab.getDisplayUri(), Uri.EMPTY);
                mAddedTab = tab;
                tabAddedLatch.countDown();
            }

            @Override
            public void onActiveTabChanged(Tab tab) {
                Assert.assertEquals(tab, mAddedTab);
                tabActivatedLatch.countDown();
            }
        }));

        runOnUiThreadBlocking(() -> mActivityTestRule.attachFragment(mFragment));
        tabAddedLatch.await();
        tabActivatedLatch.await();
    }

    @Test
    @SmallTest
    public void tabsPersistAcrossRotations() throws Exception {
        String url = getTestDataURL("simple_page.html");
        mActivityTestRule.attachNewFragmentThenNavigateAndWait(url);

        // Rotate device.
        mActivityTestRule.getActivity().setRequestedOrientation(
                ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(
                    mActivityTestRule.getActivity().getResources().getConfiguration().orientation,
                    is(ORIENTATION_LANDSCAPE));
        });

        WebFragment fragment =
                runOnUiThreadBlocking(() -> mActivityTestRule.getActivity().getAttachedFragment());
        Tab activeTab = fragment.getTabManager().get().getActiveTab().get();
        Assert.assertEquals(activeTab.getDisplayUri().toString(), url);
    }

    @Test
    @SmallTest
    public void tabsPersistAcrossSessions() throws Exception {
        FragmentParams params = (new FragmentParams.Builder())
                                        .setPersistenceId("pid1234")
                                        .setProfileName("pn12345")
                                        .build();
        mFragment = runOnUiThreadBlocking(
                () -> mActivityTestRule.getWebSandbox().createFragment(params));

        runOnUiThreadBlocking(() -> mActivityTestRule.attachFragment(mFragment));
        Tab activeTab = mFragment.getTabManager().get().getActiveTab().get();

        String url = getTestDataURL("simple_page.html");
        mActivityTestRule.navigateAndWait(activeTab, url);

        // Shutdown the sandbox.
        runOnUiThreadBlocking(() -> mActivityTestRule.detachFragment(mFragment));
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
        mFragment = runOnUiThreadBlocking(() -> sandbox.createFragment(params));
        runOnUiThreadBlocking(() -> mActivityTestRule.attachFragment(mFragment));
        Tab newActiveTab = mFragment.getTabManager().get().getActiveTab().get();

        Assert.assertEquals(newActiveTab.getDisplayUri().toString(), url);
        Assert.assertEquals(newActiveTab.getGuid(), activeTab.getGuid());
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
        CountDownLatch activeLatch = new CountDownLatch(2);
        CountDownLatch removeLatch = new CountDownLatch(1);

        final TabHolder holder = new TabHolder();
        runOnUiThreadBlocking(() -> mFragment.registerTabListObserver(new TabListObserver() {
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

        runOnUiThreadBlocking(() -> mActivityTestRule.attachFragment(mFragment));
        TabManager tabManager = mFragment.getTabManager().get();

        Tab newTab = runOnUiThreadBlocking(() -> tabManager.createTab()).get();
        Assert.assertEquals(newTab, holder.mAddedTab);

        runOnUiThreadBlocking(() -> newTab.setActive());
        activeLatch.await();
        Assert.assertEquals(newTab, holder.mActiveTab);

        runOnUiThreadBlocking(() -> newTab.close());
        removeLatch.await();
        Assert.assertEquals(newTab, holder.mRemovedTab);

        Assert.assertNull(tabManager.getActiveTab().get());
    }
}
