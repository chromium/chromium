// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webengine.test;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.app.Instrumentation;
import android.content.Intent;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.LargeTest;

import com.google.common.util.concurrent.SettableFuture;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.content_public.browser.test.util.ClickUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.webengine.Tab;
import org.chromium.webengine.WebEngine;
import org.chromium.webengine.WebEngineParams;
import org.chromium.webengine.WebSandbox;
import org.chromium.webengine.test.instrumentation_test_apk.InstrumentationActivity;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Tests for launching external intents from deep links.
 */
@DoNotBatch(reason = "Tests are testing behaviour between activities that are single task")
@RunWith(WebEngineJUnit4ClassRunner.class)
public class ExternalIntentsTest {
    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    // We need plenty time to load up a page, launch a new activity, and return.
    // We need to be confident that those events also did not happen in the disabled
    // case if this times out.
    // The is quite a slow set of tests because we need this to be a large value to avoid flaking.
    private static final int TEST_TIMEOUT_MS = 20_000;

    private EmbeddedTestServer mServer;
    private WebSandbox mSandbox;
    private Instrumentation mInstrumentation;
    private String mUrl;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.launchShell();

        mServer = mTestServerRule.getServer();
        mUrl = getTestDataURL("external_intents.html#"
                + mActivityTestRule.getShellComponentName().flattenToString());

        mSandbox = mActivityTestRule.getWebSandbox();
    }

    @After
    public void shutdown() {
        runOnUiThreadBlocking(() -> mSandbox.shutdown());
        mActivityTestRule.finish();
    }

    @Test
    @LargeTest
    @DisabledTest(message = "Flaky because this relies on waiting for an activity to launch")
    public void testOpensExternalIntents_shouldLaunch() throws Exception {
        // Awful hack heads up:
        // The problem is that the application is a separate application from the
        // activity running the tests so we can't use an ActivityMonitor.
        // Chromium thinks that if you are opening a deeplink to the same package,
        // it should act as a "tab" navigation and not send an external intent.
        // We can't override apis like startActivity for the instrumentation activity
        // because the component is starting the activity from the web engine context.
        final SettableFuture<Boolean> launchedExternal = SettableFuture.create();
        mActivityTestRule.getActivity().setLifeCycleListener(
                new InstrumentationActivity.LifeCycleListener() {
                    @Override
                    public void onNewIntent(Intent intent) {
                        launchedExternal.set(intent.hasExtra("LAUNCHED_EXTERNAL"));
                    }
                });
        Tab currentTab = createWebEngineAndLoadPage(/* isExternalIntentsEnabled= */ true);

        ClickUtils.mouseSingleClickView(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getFragmentContainerView(), 1, 1);

        try {
            Assert.assertTrue(launchedExternal.get(TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS));
        } catch (TimeoutException e) {
            Assert.fail("Shouldn't have timed out");
        }
    }

    @Test
    @LargeTest
    public void testDisableExternalIntents_shouldNotLaunch() throws Exception {
        final SettableFuture<Boolean> launchedExternal = SettableFuture.create();
        mActivityTestRule.getActivity().setLifeCycleListener(
                new InstrumentationActivity.LifeCycleListener() {
                    @Override
                    public void onNewIntent(Intent intent) {
                        launchedExternal.set(intent.hasExtra("LAUNCHED_EXTERNAL"));
                    }
                });
        Tab currentTab = createWebEngineAndLoadPage(/* isExternalIntentsEnabled= */ false);

        ClickUtils.mouseSingleClickView(InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getFragmentContainerView(), 1, 1);

        try {
            Assert.assertFalse(launchedExternal.get(TEST_TIMEOUT_MS, TimeUnit.MILLISECONDS));
            // If we get here it means the test resumed from the other activity
            Assert.fail("The activity shouldn't have been resumed");
        } catch (TimeoutException e) {
            // Yay it was meant to timeout
        }
    }

    /**
     * Creates a new web engine and loads the test page.
     *
     * Will configure if we can launch external intents or not.
     */
    private Tab createWebEngineAndLoadPage(boolean isExternalIntentsEnabled) throws Exception {
        WebEngineParams params = new WebEngineParams.Builder()
                                         .setProfileName("Default")
                                         .setIsExternalIntentsEnabled(isExternalIntentsEnabled)
                                         .build();

        WebEngine webEngine = runOnUiThreadBlocking(() -> mSandbox.createWebEngine(params)).get();
        runOnUiThreadBlocking(() -> mActivityTestRule.attachFragment(webEngine.getFragment()));

        Tab activeTab = webEngine.getTabManager().getActiveTab();
        mActivityTestRule.navigateAndWait(activeTab, mUrl);

        return activeTab;
    }

    private String getTestDataURL(String path) {
        return mServer.getURL("/weblayer/test/data/" + path);
    }
}
