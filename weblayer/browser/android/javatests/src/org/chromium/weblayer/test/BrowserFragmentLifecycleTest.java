// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.content.Intent;
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

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
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

    // Used to track new Browsers finish restoring.
    private static final class BrowserRestoreHelper
            implements InstrumentationActivity.OnCreatedCallback {
        private final CallbackHelper mCallbackHelper;
        public List<Browser> mBrowsers;

        // |helper| is notified once restore is complete (or if a browser is created already
        // restored).
        BrowserRestoreHelper(CallbackHelper helper) {
            mCallbackHelper = helper;
            mBrowsers = new ArrayList<Browser>();
            InstrumentationActivity.registerOnCreatedCallback(this);
        }

        @Override
        public void onCreated(Browser browser) {
            mBrowsers.add(browser);
            if (!browser.isRestoringPreviousState()) {
                mCallbackHelper.notifyCalled();
                return;
            }
            browser.registerBrowserRestoreCallback(new BrowserRestoreCallback() {
                @Override
                public void onRestoreCompleted() {
                    Assert.assertFalse(browser.isRestoringPreviousState());
                    browser.unregisterBrowserRestoreCallback(this);
                    mCallbackHelper.notifyCalled();
                }
            });
        }
    }
    @Test
    @SmallTest
    @MinWebLayerVersion(88)
    public void restoreUsingOnRestoreCompleted() throws Throwable {
        final String persistenceId = "x";
        Bundle extras = new Bundle();
        extras.putString(InstrumentationActivity.EXTRA_PERSISTENCE_ID, persistenceId);
        CallbackHelper callbackHelper = new CallbackHelper();
        BrowserRestoreHelper restoreHelper = new BrowserRestoreHelper(callbackHelper);

        final String url = mActivityTestRule.getTestDataURL("simple_page.html");
        mActivityTestRule.launchShellWithUrl(url, extras);
        // Wait for the restore to complete.
        callbackHelper.waitForCallback(0, 1);

        // Recreate and wait for restore.
        mActivityTestRule.recreateActivity();
        callbackHelper.waitForCallback(1, 1);
    }

    private String getCurrentDisplayUri(Browser browser) {
        NavigationController navigationController =
                browser.getActiveTab().getNavigationController();
        return navigationController
                .getNavigationEntryDisplayUri(navigationController.getNavigationListCurrentIndex())
                .toString();
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(87)
    public void twoFragmentsDifferentIncognitoProfiles() throws Throwable {
        // This test creates two browsers with different profile names and persistence ids.
        final String persistenceId1 = "x";
        final String persistenceId2 = "y";
        Bundle extras = new Bundle();
        extras.putString(InstrumentationActivity.EXTRA_PERSISTENCE_ID, persistenceId1);
        extras.putString(InstrumentationActivity.EXTRA_PROFILE_NAME, persistenceId1);
        extras.putBoolean(InstrumentationActivity.EXTRA_IS_INCOGNITO, true);
        CallbackHelper callbackHelper = new CallbackHelper();
        BrowserRestoreHelper restoreHelper = new BrowserRestoreHelper(callbackHelper);
        final String url1 = mActivityTestRule.getTestDataURL("simple_page.html");
        mActivityTestRule.launchShellWithUrl(url1, extras);

        // Wait for the restore to complete.
        int currentCallCount = 0;
        callbackHelper.waitForCallback(currentCallCount++, 1);

        // Create another fragment
        Browser newBrowser = TestThreadUtils.runOnUiThreadBlocking(() -> {
            InstrumentationActivity activity = mActivityTestRule.getActivity();
            Intent intent = new Intent(activity.getIntent());
            intent.putExtra(InstrumentationActivity.EXTRA_PERSISTENCE_ID, persistenceId2);
            intent.putExtra(InstrumentationActivity.EXTRA_PROFILE_NAME, persistenceId2);

            // The newly created browser should have a different Profile, but be incognito.
            Browser browser = Browser.fromFragment(
                    activity.createBrowserFragment(android.R.id.content, intent));
            Assert.assertNotEquals(browser, activity.getBrowser());
            Assert.assertNotEquals(browser.getProfile(), activity.getBrowser().getProfile());
            Assert.assertTrue(activity.getBrowser().getProfile().isIncognito());
            Assert.assertTrue(browser.getProfile().isIncognito());
            return browser;
        });

        // Wait for restore.
        callbackHelper.waitForCallback(currentCallCount++, 1);

        // Navigate to url2.
        final String url2 = mActivityTestRule.getTestDataURL("simple_page2.html");
        Tab newTab = TestThreadUtils.runOnUiThreadBlocking(() -> newBrowser.getActiveTab());
        mActivityTestRule.navigateAndWait(newTab, url2, true);

        Profile[] profiles = new Profile[2];
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            profiles[0] = mActivityTestRule.getActivity().getBrowser().getProfile();
            profiles[1] = newBrowser.getProfile();
        });

        // Recreate the activity and wait for two restores (for the two fragments).
        InstrumentationActivity.sAllowMultipleFragments = true;
        restoreHelper.mBrowsers.clear();
        mActivityTestRule.recreateActivity();
        callbackHelper.waitForCallback(currentCallCount++, 1);
        callbackHelper.waitForCallback(currentCallCount++, 1);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Two new Browsers should be created, but have the profiles created earlier.
            Assert.assertEquals(2, restoreHelper.mBrowsers.size());
            Browser restoredBrowser1 = restoreHelper.mBrowsers.get(0);
            Browser restoredBrowser2 = restoreHelper.mBrowsers.get(1);
            if (restoredBrowser2.getProfile().getName().equals(persistenceId1)) {
                restoredBrowser1 = restoreHelper.mBrowsers.get(1);
                restoredBrowser2 = restoreHelper.mBrowsers.get(0);
            }
            Assert.assertEquals(restoredBrowser1.getProfile().getName(), persistenceId1);
            Assert.assertTrue(restoredBrowser1.getProfile().isIncognito());
            Assert.assertEquals(profiles[0], restoredBrowser1.getProfile());
            Assert.assertEquals(url1, getCurrentDisplayUri(restoredBrowser1));

            Assert.assertEquals(restoredBrowser2.getProfile().getName(), persistenceId2);
            Assert.assertTrue(restoredBrowser2.getProfile().isIncognito());
            Assert.assertEquals(profiles[1], restoredBrowser2.getProfile());
            Assert.assertEquals(url2, getCurrentDisplayUri(restoredBrowser2));
            Assert.assertNotEquals(restoredBrowser2, newBrowser);
        });
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(87)
    public void twoFragmentsSameIncognitoProfile() throws Throwable {
        // This test creates two browsers with the same profile, but different persistence ids.
        final String persistenceId1 = "x";
        final String persistenceId2 = "y";
        Bundle extras = new Bundle();
        extras.putString(InstrumentationActivity.EXTRA_PERSISTENCE_ID, persistenceId1);
        extras.putString(InstrumentationActivity.EXTRA_PROFILE_NAME, persistenceId1);
        extras.putBoolean(InstrumentationActivity.EXTRA_IS_INCOGNITO, true);
        CallbackHelper callbackHelper = new CallbackHelper();
        BrowserRestoreHelper restoreHelper = new BrowserRestoreHelper(callbackHelper);

        final String url1 = mActivityTestRule.getTestDataURL("simple_page.html");
        mActivityTestRule.launchShellWithUrl(url1, extras);
        // Wait for the restore to complete.
        int currentCallCount = 0;
        callbackHelper.waitForCallback(currentCallCount++, 1);

        // Create another fragment
        Browser newBrowser = TestThreadUtils.runOnUiThreadBlocking(() -> {
            InstrumentationActivity activity = mActivityTestRule.getActivity();
            Intent intent = new Intent(activity.getIntent());
            intent.putExtra(InstrumentationActivity.EXTRA_PERSISTENCE_ID, persistenceId2);

            // The newly created browser should have a different Profile, but be incognito.
            Browser browser = Browser.fromFragment(
                    activity.createBrowserFragment(android.R.id.content, intent));
            Assert.assertNotEquals(browser, activity.getBrowser());
            Assert.assertEquals(browser.getProfile(), activity.getBrowser().getProfile());
            Assert.assertTrue(activity.getBrowser().getProfile().isIncognito());
            return browser;
        });
        Profile profile = TestThreadUtils.runOnUiThreadBlocking(() -> newBrowser.getProfile());
        // Wait for restore.
        callbackHelper.waitForCallback(currentCallCount++, 1);

        // Navigate to url2.
        final String url2 = mActivityTestRule.getTestDataURL("simple_page2.html");
        Tab newTab = TestThreadUtils.runOnUiThreadBlocking(() -> newBrowser.getActiveTab());
        mActivityTestRule.navigateAndWait(newTab, url2, true);

        // Recreate the activity and wait for two restores (for the two fragments).
        InstrumentationActivity.sAllowMultipleFragments = true;
        restoreHelper.mBrowsers.clear();
        mActivityTestRule.recreateActivity();
        callbackHelper.waitForCallback(currentCallCount++, 1);
        callbackHelper.waitForCallback(currentCallCount++, 1);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Two new Browsers should be created.
            Assert.assertEquals(2, restoreHelper.mBrowsers.size());
            Browser restoredBrowser1 = restoreHelper.mBrowsers.get(0);
            Browser restoredBrowser2 = restoreHelper.mBrowsers.get(1);
            Assert.assertEquals(profile, restoredBrowser1.getProfile());

            Assert.assertEquals(profile, restoredBrowser2.getProfile());
            Assert.assertNotEquals(restoredBrowser2, newBrowser);

            if (getCurrentDisplayUri(restoredBrowser1).equals(url1)) {
                Assert.assertEquals(url2, getCurrentDisplayUri(restoredBrowser2));
            } else {
                Assert.assertEquals(url1, getCurrentDisplayUri(restoredBrowser2));
                Assert.assertEquals(url2, getCurrentDisplayUri(restoredBrowser1));
            }
        });
    }
}
