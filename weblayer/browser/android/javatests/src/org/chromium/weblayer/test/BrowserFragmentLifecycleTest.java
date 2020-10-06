// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.net.Uri;
import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.fragment.app.FragmentManager;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Browser;
import org.chromium.weblayer.BrowserRestoreCallback;
import org.chromium.weblayer.Navigation;
import org.chromium.weblayer.NavigationCallback;
import org.chromium.weblayer.NavigationController;
import org.chromium.weblayer.Profile;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.WebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Tests that fragment lifecycle works as expected.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class BrowserFragmentLifecycleTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private Tab getTab() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mActivityTestRule.getActivity().getTab());
    }

    private boolean isRestoringPreviousState() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> mActivityTestRule.getActivity().getBrowser().isRestoringPreviousState());
    }

    private int getSupportedMajorVersion() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> WebLayer.getSupportedMajorVersion(mActivityTestRule.getActivity()));
    }

    @Test
    @SmallTest
    public void successfullyLoadsUrlAfterRecreation() {
        mActivityTestRule.launchShellWithUrl("about:blank");
        String url = "data:text,foo";
        mActivityTestRule.navigateAndWait(getTab(), url, false);

        mActivityTestRule.recreateActivity();

        url = "data:text,bar";
        mActivityTestRule.navigateAndWait(getTab(), url, false);
    }

    @Test
    @SmallTest
    public void restoreAfterRecreate() throws Throwable {
        mActivityTestRule.launchShellWithUrl("about:blank");
        String url = "data:text,foo";
        mActivityTestRule.navigateAndWait(getTab(), url, false);

        mActivityTestRule.recreateActivity();

        waitForTabToFinishRestore(getTab(), url);
    }

    private void destroyFragment(CallbackHelper helper) {
        FragmentManager fm = mActivityTestRule.getActivity().getSupportFragmentManager();
        fm.beginTransaction()
                .remove(fm.getFragments().get(0))
                .runOnCommit(helper::notifyCalled)
                .commit();
    }

    // https://crbug.com/1021041
    @Test
    @SmallTest
    public void handlesFragmentDestroyWhileNavigating() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        CallbackHelper helper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            NavigationController navigationController = activity.getTab().getNavigationController();
            navigationController.registerNavigationCallback(new NavigationCallback() {
                @Override
                public void onReadyToCommitNavigation(@NonNull Navigation navigation) {
                    destroyFragment(helper);
                }
            });
            navigationController.navigate(Uri.parse("data:text,foo"));
        });
        helper.waitForFirst();
    }

    // Waits for |tab| to finish loadding |url. This is intended to be called after restore.
    private void waitForTabToFinishRestore(Tab tab, String url) {
        BoundedCountDownLatch latch = new BoundedCountDownLatch(1);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // It's possible the NavigationController hasn't loaded yet, handle either scenario.
            NavigationController navigationController = tab.getNavigationController();
            for (int i = 0; i < navigationController.getNavigationListSize(); ++i) {
                if (navigationController.getNavigationEntryDisplayUri(i).equals(Uri.parse(url))) {
                    latch.countDown();
                    return;
                }
            }
            navigationController.registerNavigationCallback(new NavigationCallback() {
                @Override
                public void onNavigationCompleted(@NonNull Navigation navigation) {
                    if (navigation.getUri().equals(Uri.parse(url))) {
                        latch.countDown();
                    }
                }
            });
        });
        latch.timedAwait();
    }

    // Recreates the activity and waits for the first tab to be restored. |extras| is the Bundle
    // used to launch the shell.
    private void restoresPreviousSession(Bundle extras) {
        extras.putString(InstrumentationActivity.EXTRA_PERSISTENCE_ID, "x");
        final String url = mActivityTestRule.getTestDataURL("simple_page.html");
        mActivityTestRule.launchShellWithUrl(url, extras);
        if (getSupportedMajorVersion() >= 88) {
            Assert.assertFalse(isRestoringPreviousState());
        }

        mActivityTestRule.recreateActivity();

        Tab tab = getTab();
        Assert.assertNotNull(tab);
        waitForTabToFinishRestore(tab, url);
        if (getSupportedMajorVersion() >= 88) {
            Assert.assertFalse(isRestoringPreviousState());
        }
    }

    @Test
    @SmallTest
    public void restoresPreviousSession() throws Throwable {
        restoresPreviousSession(new Bundle());
    }

    @Test
    @SmallTest
    public void restoresPreviousSessionIncognito() throws Throwable {
        Bundle extras = new Bundle();
        // This forces incognito.
        extras.putString(InstrumentationActivity.EXTRA_PROFILE_NAME, null);
        restoresPreviousSession(extras);
    }

    @Test
    @SmallTest
    public void restoresTabGuid() throws Throwable {
        Bundle extras = new Bundle();
        extras.putString(InstrumentationActivity.EXTRA_PERSISTENCE_ID, "x");
        final String url = mActivityTestRule.getTestDataURL("simple_page.html");
        mActivityTestRule.launchShellWithUrl(url, extras);
        final String initialTabId = TestThreadUtils.runOnUiThreadBlocking(
                () -> { return mActivityTestRule.getActivity().getTab().getGuid(); });
        Assert.assertNotNull(initialTabId);
        Assert.assertFalse(initialTabId.isEmpty());

        mActivityTestRule.recreateActivity();

        Tab tab = getTab();
        Assert.assertNotNull(tab);
        waitForTabToFinishRestore(tab, url);
        final String restoredTabId =
                TestThreadUtils.runOnUiThreadBlockingNoException(() -> { return tab.getGuid(); });
        Assert.assertEquals(initialTabId, restoredTabId);
    }

    @Test
    @SmallTest
    public void restoreTabGuidAfterRecreate() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        final Tab tab = getTab();
        final String initialTabId =
                TestThreadUtils.runOnUiThreadBlocking(() -> { return tab.getGuid(); });
        String url = "data:text,foo";
        mActivityTestRule.navigateAndWait(tab, url, false);

        mActivityTestRule.recreateActivity();

        final Tab restoredTab = getTab();
        Assert.assertNotEquals(tab, restoredTab);
        waitForTabToFinishRestore(restoredTab, url);
        final String restoredTabId = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> { return restoredTab.getGuid(); });
        Assert.assertEquals(initialTabId, restoredTabId);
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(85)
    public void restoresTabData() throws Throwable {
        Bundle extras = new Bundle();
        extras.putString(InstrumentationActivity.EXTRA_PERSISTENCE_ID, "x");

        Map<String, String> initialData = new HashMap<>();
        initialData.put("foo", "bar");
        restoreTabData(extras, initialData);
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(85)
    public void restoreTabDataAfterRecreate() throws Throwable {
        Map<String, String> initialData = new HashMap<>();
        initialData.put("foo", "bar");
        restoreTabData(new Bundle(), initialData);
    }

    private void restoreTabData(Bundle extras, Map<String, String> initialData) {
        String url = mActivityTestRule.getTestDataURL("simple_page.html");
        mActivityTestRule.launchShellWithUrl(url, extras);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = mActivityTestRule.getActivity().getTab();
            Assert.assertTrue(tab.getData().isEmpty());
            tab.setData(initialData);
        });

        mActivityTestRule.recreateActivity();

        Tab tab = getTab();
        Assert.assertNotNull(tab);
        waitForTabToFinishRestore(tab, url);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> Assert.assertEquals(initialData, tab.getData()));
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(85)
    public void getAndRemoveBrowserPersistenceIds() throws Throwable {
        // Creates a browser with the persistence id 'x'.
        final String persistenceId = "x";
        Bundle extras = new Bundle();
        extras.putString(InstrumentationActivity.EXTRA_PERSISTENCE_ID, persistenceId);
        final String url = mActivityTestRule.getTestDataURL("simple_page.html");
        mActivityTestRule.launchShellWithUrl(url, extras);

        // Destroy the frament, which ensures the persistence file was written to.
        CallbackHelper helper = new CallbackHelper();
        Profile profile = TestThreadUtils.runOnUiThreadBlocking(
                () -> { return mActivityTestRule.getActivity().getBrowser().getProfile(); });
        TestThreadUtils.runOnUiThreadBlocking(() -> destroyFragment(helper));
        helper.waitForCallback(0, 1);
        int callCount = helper.getCallCount();

        // Verify the id can be fetched.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            profile.getBrowserPersistenceIds((Set<String> ids) -> {
                Assert.assertEquals(1, ids.size());
                Assert.assertTrue(ids.contains(persistenceId));
                helper.notifyCalled();
            });
        });
        helper.waitForCallback(callCount, 1);
        callCount = helper.getCallCount();

        // Remove the storage.
        HashSet<String> ids = new HashSet<String>();
        ids.add("x");
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            profile.removeBrowserPersistenceStorage(ids, (Boolean result) -> {
                Assert.assertTrue(result);
                helper.notifyCalled();
            });
        });
        helper.waitForCallback(callCount, 1);
        callCount = helper.getCallCount();

        // Verify it was actually removed.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            profile.getBrowserPersistenceIds((Set<String> actualIds) -> {
                Assert.assertTrue(actualIds.isEmpty());
                helper.notifyCalled();
            });
        });
        helper.waitForCallback(callCount, 1);
    }

    @Test
    @SmallTest
    public void browserAndTabIsDestroyedWhenFragmentDestroyed() throws Throwable {
        mActivityTestRule.launchShellWithUrl(mActivityTestRule.getTestDataURL("simple_page.html"));

        CallbackHelper helper = new CallbackHelper();
        Browser browser = TestThreadUtils.runOnUiThreadBlocking(
                () -> { return mActivityTestRule.getActivity().getBrowser(); });
        Tab tab = TestThreadUtils.runOnUiThreadBlocking(() -> { return browser.getActiveTab(); });
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertFalse(browser.isDestroyed());
            Assert.assertFalse(tab.isDestroyed());
            destroyFragment(helper);
        });
        helper.waitForFirst();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue(browser.isDestroyed());
            Assert.assertTrue(tab.isDestroyed());
        });
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(88)
    public void restoreUsingOnRestoreCompleted() throws Throwable {
        final String persistenceId = "x";
        Bundle extras = new Bundle();
        extras.putString(InstrumentationActivity.EXTRA_PERSISTENCE_ID, persistenceId);
        CallbackHelper callbackHelper = new CallbackHelper();
        InstrumentationActivity.registerOnCreatedCallback(
                new InstrumentationActivity.OnCreatedCallback() {
                    private int mBrowserCreateCount;
                    @Override
                    public void onCreated(Browser browser) {
                        if (mBrowserCreateCount == 0) {
                            // Initial creation.
                            mBrowserCreateCount = 1;
                            // isRestoringPreviousState() is true for the initial creation as
                            // persistence code has to check disk, which is async.
                            Assert.assertTrue(browser.isRestoringPreviousState());
                        } else if (mBrowserCreateCount == 1) {
                            // The activity was recreated.
                            mBrowserCreateCount = 2;
                            Assert.assertTrue(browser.isRestoringPreviousState());
                            browser.registerBrowserRestoreCallback(new BrowserRestoreCallback() {
                                @Override
                                public void onRestoreCompleted() {
                                    Assert.assertFalse(browser.isRestoringPreviousState());
                                    callbackHelper.notifyCalled();
                                }
                            });
                        } else {
                            Assert.fail("Unexpected phase");
                        }
                    }
                });
        final String url = mActivityTestRule.getTestDataURL("simple_page.html");
        mActivityTestRule.launchShellWithUrl(url, extras);

        mActivityTestRule.recreateActivity();
        callbackHelper.waitForFirst();
    }
}
