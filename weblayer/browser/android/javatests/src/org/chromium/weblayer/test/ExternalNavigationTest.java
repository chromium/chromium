// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;

import androidx.annotation.NonNull;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Browser;
import org.chromium.weblayer.Callback;
import org.chromium.weblayer.ExternalIntentInIncognitoCallback;
import org.chromium.weblayer.ExternalIntentInIncognitoUserDecision;
import org.chromium.weblayer.NavigateParams;
import org.chromium.weblayer.Navigation;
import org.chromium.weblayer.NavigationCallback;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TabListCallback;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests handling of external intents.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class ExternalNavigationTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private static final boolean EXPECT_NAVIGATION_COMPLETION = true;
    private static final boolean EXPECT_NAVIGATION_FAILURE = false;
    private static final boolean RESULTS_IN_EXTERNAL_INTENT = true;
    private static final boolean DOESNT_RESULT_IN_EXTERNAL_INTENT = false;
    private static final boolean RESULTS_IN_USER_DECIDING_EXTERNAL_INTENT = true;
    private static final boolean DOESNT_RESULT_IN_USER_DECIDING_EXTERNAL_INTENT = false;

    private static final String ABOUT_BLANK_URL = "about:blank";
    private static final String CUSTOM_SCHEME_URL_WITH_DEFAULT_EXTERNAL_HANDLER =
            "weblayer://weblayertest/intent";
    private static final String INTENT_TO_DUMMY_ACTIVITY_FOR_SPECIAL_SCHEME_DATA_STRING =
            CUSTOM_SCHEME_URL_WITH_DEFAULT_EXTERNAL_HANDLER;
    private static final String INTENT_TO_DUMMY_ACTIVITY_FOR_SPECIAL_SCHEME_ACTION =
            "android.intent.action.VIEW";
    // The package is not specified in the intent that gets created when navigating to the special
    // scheme.
    private static final String INTENT_TO_DUMMY_ACTIVITY_FOR_SPECIAL_SCHEME_PACKAGE = null;
    private static final String INTENT_TO_SELF_DATA_CONTENT = "example.test";
    private static final String INTENT_TO_SELF_SCHEME = "https";
    private static final String INTENT_TO_SELF_DATA_STRING =
            INTENT_TO_SELF_SCHEME + "://" + INTENT_TO_SELF_DATA_CONTENT;
    private static final String INTENT_TO_SELF_ACTION = "android.intent.action.VIEW";
    private static final String INTENT_TO_SELF_PACKAGE =
            InstrumentationRegistry.getInstrumentation().getTargetContext().getPackageName();

    // An intent that opens the test app to view a specified URL. Note that the "end" is left off to
    // allow appending extras when constructing URLs.
    private static final String INTENT_TO_SELF = "intent://" + INTENT_TO_SELF_DATA_CONTENT
            + "#Intent;scheme=" + INTENT_TO_SELF_SCHEME + ";action=" + INTENT_TO_SELF_ACTION
            + ";package=" + INTENT_TO_SELF_PACKAGE + ";";
    private static final String INTENT_TO_SELF_URL = INTENT_TO_SELF + "end";

    // An intent URL that gets rejected as malformed.
    private static final String MALFORMED_INTENT_URL = "intent://garbage;end";

    // An intent that is properly formed but wishes to open an app that is not present on the
    // device. Note that the "end" is left off to allow appending extras when constructing URLs.
    private static final String NON_RESOLVABLE_INTENT =
            "intent://dummy.com/#Intent;scheme=https;action=android.intent.action.VIEW;package=com.missing.app;";

    private static final String LINK_WITH_INTENT_TO_SELF_IN_SAME_TAB_FILE =
            "link_with_intent_to_package_in_same_tab.html#" + INTENT_TO_SELF_PACKAGE;
    private static final String LINK_WITH_INTENT_TO_SELF_IN_NEW_TAB_FILE =
            "link_with_intent_to_package_in_new_tab.html#" + INTENT_TO_SELF_PACKAGE;
    private static final String PAGE_THAT_INTENTS_TO_CHROME_ON_LOAD_FILE =
            "page_that_intents_to_package_on_load.html#" + INTENT_TO_SELF_PACKAGE;
    private static final String LINK_TO_PAGE_THAT_INTENTS_TO_CHROME_ON_LOAD_FILE =
            "link_to_page_that_intents_to_package_on_load.html#" + INTENT_TO_SELF_PACKAGE;

    // The test server handles "echo" with a response containing "Echo" :).
    private final String mTestServerSiteUrl = mActivityTestRule.getTestServer().getURL("/echo");

    private final String mTestServerSiteFallbackUrlExtra =
            "S.browser_fallback_url=" + android.net.Uri.encode(mTestServerSiteUrl) + ";";
    private final String mIntentToSelfWithFallbackUrl =
            INTENT_TO_SELF + mTestServerSiteFallbackUrlExtra + "end";
    private final String mNonResolvableIntentWithFallbackUrl =
            NON_RESOLVABLE_INTENT + mTestServerSiteFallbackUrlExtra + "end";

    private final String mRedirectToCustomSchemeUrlWithDefaultExternalHandler =
            mActivityTestRule.getTestServer().getURL(
                    "/server-redirect?" + CUSTOM_SCHEME_URL_WITH_DEFAULT_EXTERNAL_HANDLER);
    private final String mRedirectToIntentToSelfURL =
            mActivityTestRule.getTestServer().getURL("/server-redirect?" + INTENT_TO_SELF_URL);
    private final String mNonResolvableIntentWithFallbackUrlThatLaunchesIntent =
            NON_RESOLVABLE_INTENT + "S.browser_fallback_url="
            + android.net.Uri.encode(mRedirectToIntentToSelfURL) + ";end";

    private static final String SPECIALIZED_DATA_URL = "data://externalnavtest";

    private class IntentInterceptor implements InstrumentationActivity.IntentInterceptor {
        public Intent mLastIntent;
        private CallbackHelper mCallbackHelper = new CallbackHelper();

        @Override
        public void interceptIntent(Intent intent, int requestCode, Bundle options) {
            mLastIntent = intent;
            mCallbackHelper.notifyCalled();
        }

        public void waitForIntent() {
            try {
                mCallbackHelper.waitForFirst();
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }
    }

    private class ExternalIntentInIncognitoCallbackTestImpl
            extends ExternalIntentInIncognitoCallback {
        private Callback<Integer> mOnUserDecisionCallback;
        CallbackHelper mCallbackHelper = new CallbackHelper();

        @Override
        public void onExternalIntentInIncognito(@NonNull Callback<Integer> onUserDecisionCallback) {
            mOnUserDecisionCallback = onUserDecisionCallback;
            mCallbackHelper.notifyCalled();
        }

        public Callback<Integer> getOnUserDecisionCallback() {
            return mOnUserDecisionCallback;
        }

        public void waitForNotificationOnExternalIntentLaunch() throws Throwable {
            mCallbackHelper.waitForFirst();
        }
    }

    /*
     * Navigates to |urlToNavigateTo| and waits for a completed/failed navigation to |urlToWaitFor|
     * as appropriate. In the callback verifies that the values of the relevant params on the
     * Navigation match the passed-in expected values.
     */
    private void navigateAndCheckExternalIntentParams(String urlToNavigateTo, String urlToWaitFor,
            boolean expectNavigationCompletion, boolean resultsInExternalIntent,
            boolean resultsInUserDecidingIntentLaunch) throws Throwable {
        Tab tab = mActivityTestRule.getActivity().getTab();

        CallbackHelper navigationCompletedCallbackHelper = new CallbackHelper();
        CallbackHelper navigationFailedCallbackHelper = new CallbackHelper();

        NavigationCallback navigationCallback = new NavigationCallback() {
            @Override
            public void onNavigationCompleted(Navigation navigation) {
                String url = navigation.getUri().toString();
                if (!url.equals(urlToWaitFor)) return;

                Assert.assertEquals(true, expectNavigationCompletion);

                // A navigation should never be expected to both complete and result in an external
                // intent.
                Assert.assertEquals(false, resultsInExternalIntent);
                Assert.assertEquals(false, navigation.wasIntentLaunched());
                Assert.assertEquals(false, resultsInUserDecidingIntentLaunch);
                Assert.assertEquals(false, navigation.isUserDecidingIntentLaunch());

                navigationCompletedCallbackHelper.notifyCalled();
            }

            @Override
            public void onNavigationFailed(Navigation navigation) {
                String url = navigation.getUri().toString();
                if (!url.equals(urlToWaitFor)) return;

                Assert.assertEquals(false, expectNavigationCompletion);

                Assert.assertEquals(resultsInExternalIntent, navigation.wasIntentLaunched());
                Assert.assertEquals(
                        resultsInUserDecidingIntentLaunch, navigation.isUserDecidingIntentLaunch());

                navigationFailedCallbackHelper.notifyCalled();
            }
        };

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tab.getNavigationController().registerNavigationCallback(navigationCallback);
            tab.getNavigationController().navigate(Uri.parse(urlToNavigateTo));
        });

        if (expectNavigationCompletion) {
            navigationCompletedCallbackHelper.waitForFirst();
        } else {
            navigationFailedCallbackHelper.waitForFirst();
        }

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tab.getNavigationController().unregisterNavigationCallback(navigationCallback);
        });
    }

    /*
     * A convenience variant of the above method that navigates to and waits for the same URL. See
     * comments on the above method.
     */
    private void navigateAndCheckExternalIntentParams(String urlToNavigateTo,
            boolean expectNavigationCompletion, boolean resultsInExternalIntent,
            boolean resultsInUserDecidingIntentLaunch) throws Throwable {
        navigateAndCheckExternalIntentParams(urlToNavigateTo, urlToNavigateTo,
                expectNavigationCompletion, resultsInExternalIntent,
                resultsInUserDecidingIntentLaunch);
    }

    /**
     * Verifies that for a navigation to a URI that WebLayer can handle internally, there
     * is no external intent triggered.
     */
    @Test
    @SmallTest
    public void testBrowserNavigation() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        mActivityTestRule.navigateAndWait(mTestServerSiteUrl);

        Assert.assertNull(intentInterceptor.mLastIntent);
        Assert.assertEquals(mTestServerSiteUrl, mActivityTestRule.getCurrentDisplayUrl());
    }

    /**
     * Tests that a direct navigation to an external intent in a background tab is blocked.
     */
    @Test
    @SmallTest
    public void testExternalIntentWithNoRedirectInBackgroundTabBlockedByDefault() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        Tab backgroundTab = TestThreadUtils.runOnUiThreadBlocking(
                () -> activity.getTab().getBrowser().createTab());
        Tab activeTab = TestThreadUtils.runOnUiThreadBlocking(
                () -> { return activity.getTab().getBrowser().getActiveTab(); });
        Assert.assertNotEquals(backgroundTab, activeTab);

        // Navigate directly to an intent in the background and verify that the intent is not
        // launched.
        NavigationWaiter waiter = new NavigationWaiter(INTENT_TO_SELF_URL, backgroundTab,
                /*expectFailure=*/true, /*waitForPaint=*/false);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            backgroundTab.getNavigationController().navigate(Uri.parse(INTENT_TO_SELF_URL));
        });

        waiter.waitForNavigation();

        Assert.assertNull(intentInterceptor.mLastIntent);
        int numNavigationsInBackgroundTab = TestThreadUtils.runOnUiThreadBlocking(
                () -> { return backgroundTab.getNavigationController().getNavigationListSize(); });
        Assert.assertEquals(0, numNavigationsInBackgroundTab);
    }

    /**
     * Tests that a direct navigation to an external intent in a background tab is launched when
     * intent launches are allowed in the background for this navigation.
     */
    @Test
    @SmallTest
    @MinWebLayerVersion(89)
    public void
    testExternalIntentWithNoRedirectInBackgroundTabLaunchedWhenBackgroundLaunchesAllowed()
            throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        Tab backgroundTab = TestThreadUtils.runOnUiThreadBlocking(
                () -> activity.getTab().getBrowser().createTab());
        Tab activeTab = TestThreadUtils.runOnUiThreadBlocking(
                () -> { return activity.getTab().getBrowser().getActiveTab(); });
        Assert.assertNotEquals(backgroundTab, activeTab);

        // Put a initial navigation in the background tab to ease verification of state
        // afterward (note that this navigation will not result in a paint due to the tab being in
        // the background).
        mActivityTestRule.navigateAndWait(backgroundTab, ABOUT_BLANK_URL, /*waitForPaint=*/false);

        // Navigate directly to an intent in the background tab with intent launching in the
        // background allowed and verify that the intent is launched.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            NavigateParams.Builder navigateParamsBuilder = new NavigateParams.Builder();
            navigateParamsBuilder.allowIntentLaunchesInBackground();
            backgroundTab.getNavigationController().navigate(
                    Uri.parse(INTENT_TO_SELF_URL), navigateParamsBuilder.build());
        });

        intentInterceptor.waitForIntent();

        // The intent should have been launched, and there should still be only the initial
        // navigation in the background tab.
        Intent intent = intentInterceptor.mLastIntent;
        Assert.assertNotNull(intent);
        Assert.assertEquals(INTENT_TO_SELF_PACKAGE, intent.getPackage());
        Assert.assertEquals(INTENT_TO_SELF_ACTION, intent.getAction());
        Assert.assertEquals(INTENT_TO_SELF_DATA_STRING, intent.getDataString());

        int numNavigationsInBackgroundTab = TestThreadUtils.runOnUiThreadBlocking(
                () -> { return backgroundTab.getNavigationController().getNavigationListSize(); });
        Assert.assertEquals(1, numNavigationsInBackgroundTab);
    }

    /**
     * Tests that a redirect to an external intent in a background tab is blocked.
     */
    @Test
    @SmallTest
    public void testExternalIntentAfterRedirectInBackgroundTabBlockedByDefault() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        Tab backgroundTab = TestThreadUtils.runOnUiThreadBlocking(
                () -> activity.getTab().getBrowser().createTab());
        Tab activeTab = TestThreadUtils.runOnUiThreadBlocking(
                () -> { return activity.getTab().getBrowser().getActiveTab(); });
        Assert.assertNotEquals(backgroundTab, activeTab);

        // Perform a navigation that redirects to an intent in the background and verify that the
        // intent is not launched.
        NavigationWaiter waiter = new NavigationWaiter(INTENT_TO_SELF_URL, backgroundTab,
                /*expectFailure=*/true, /*waitForPaint=*/false);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            backgroundTab.getNavigationController().navigate(Uri.parse(mRedirectToIntentToSelfURL));
        });

        waiter.waitForNavigation();

        Assert.assertNull(intentInterceptor.mLastIntent);
        int numNavigationsInBackgroundTab = TestThreadUtils.runOnUiThreadBlocking(
                () -> { return backgroundTab.getNavigationController().getNavigationListSize(); });
        Assert.assertEquals(0, numNavigationsInBackgroundTab);
    }

    /**
     * Tests that a redirect to an external intent in a background tab is launched when
     * intent launches are allowed in the background for this navigation.
     */
    @Test
    @SmallTest
    @MinWebLayerVersion(89)
    public void
    testExternalIntentAfterRedirectInBackgroundTabLaunchedWhenBackgroundLaunchesAllowed()
            throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        Tab backgroundTab = TestThreadUtils.runOnUiThreadBlocking(
                () -> activity.getTab().getBrowser().createTab());
        Tab activeTab = TestThreadUtils.runOnUiThreadBlocking(
                () -> { return activity.getTab().getBrowser().getActiveTab(); });
        Assert.assertNotEquals(backgroundTab, activeTab);

        // Put a initial navigation in the background tab to ease verification of state
        // afterward (note that this navigation will not result in a paint due to the tab being in
        // the background).
        mActivityTestRule.navigateAndWait(backgroundTab, ABOUT_BLANK_URL, /*waitForPaint=*/false);

        // Perform a navigation that redirects to an intent in the background tab with intent
        // launching in the background allowed and verify that the intent is launched.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            NavigateParams.Builder navigateParamsBuilder = new NavigateParams.Builder();
            navigateParamsBuilder.allowIntentLaunchesInBackground();
            backgroundTab.getNavigationController().navigate(
                    Uri.parse(mRedirectToIntentToSelfURL), navigateParamsBuilder.build());
        });

        intentInterceptor.waitForIntent();

        // The intent should have been launched, and there should still be only the initial
        // navigation in the background tab.
        Intent intent = intentInterceptor.mLastIntent;
        Assert.assertNotNull(intent);
        Assert.assertEquals(INTENT_TO_SELF_PACKAGE, intent.getPackage());
        Assert.assertEquals(INTENT_TO_SELF_ACTION, intent.getAction());
        Assert.assertEquals(INTENT_TO_SELF_DATA_STRING, intent.getDataString());

        int numNavigationsInBackgroundTab = TestThreadUtils.runOnUiThreadBlocking(
                () -> { return backgroundTab.getNavigationController().getNavigationListSize(); });
        Assert.assertEquals(1, numNavigationsInBackgroundTab);
    }

    /**
     * Tests that a direct navigation to an external intent in browser startup is blocked as the
     * browser is not yet attached to the window at the time of the navigation and thus the tab is
     * not visible.
     */
    @Test
    @SmallTest
    public void testExternalIntentWithNoRedirectInBrowserStartupBlockedByDefault()
            throws Throwable {
        CallbackHelper onNavigationFailedCallbackHelper = new CallbackHelper();
        NavigationCallback navigationCallback = new NavigationCallback() {
            @Override
            public void onNavigationFailed(Navigation navigation) {
                if (navigation.getUri().toString().equals(INTENT_TO_SELF_URL)) {
                    onNavigationFailedCallbackHelper.notifyCalled();
                }
            }
        };

        // The flow being tested is where the navigation occurs synchronously with initial browser
        // creation.
        final IntentInterceptor intentInterceptor = new IntentInterceptor();
        InstrumentationActivity.registerOnCreatedCallback(
                new InstrumentationActivity.OnCreatedCallback() {
                    @Override
                    public void onCreated(Browser browser, InstrumentationActivity activity) {
                        activity.setIntentInterceptor(intentInterceptor);
                        browser.getActiveTab().getNavigationController().registerNavigationCallback(
                                navigationCallback);
                        browser.getActiveTab().getNavigationController().navigate(
                                Uri.parse(INTENT_TO_SELF_URL));
                    }
                });

        mActivityTestRule.launchShell(new Bundle());

        // The navigation should fail...
        onNavigationFailedCallbackHelper.waitForFirst();

        // ...the intent should not have been launched...
        Assert.assertNull(intentInterceptor.mLastIntent);

        // ...and there should be one tab in the browser without any navigations in it.
        Browser browser = mActivityTestRule.getActivity().getBrowser();
        int numTabs =
                TestThreadUtils.runOnUiThreadBlocking(() -> { return browser.getTabs().size(); });
        Assert.assertEquals(1, numTabs);
        int numNavigationsInTab = TestThreadUtils.runOnUiThreadBlocking(() -> {
            return browser.getActiveTab().getNavigationController().getNavigationListSize();
        });
        Assert.assertEquals(0, numNavigationsInTab);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            browser.getActiveTab().getNavigationController().unregisterNavigationCallback(
                    navigationCallback);
        });
    }

    /**
     * Tests that a direct navigation to an external intent in browser startup is launched if the
     * embedder specifies that intent launches in the background are allowed for this navigation.
     */
    @Test
    @SmallTest
    @MinWebLayerVersion(89)
    public void
    testExternalIntentWithNoRedirectInBrowserStartupLaunchedWhenBackgroundLaunchesAllowed()
            throws Throwable {
        CallbackHelper onTabRemovedCallbackHelper = new CallbackHelper();
        TabListCallback tabListCallback = new TabListCallback() {
            @Override
            public void onTabRemoved(Tab tab) {
                onTabRemovedCallbackHelper.notifyCalled();
            }
        };

        final IntentInterceptor intentInterceptor = new IntentInterceptor();
        InstrumentationActivity.registerOnCreatedCallback(
                new InstrumentationActivity.OnCreatedCallback() {
                    @Override
                    public void onCreated(Browser browser, InstrumentationActivity activity) {
                        activity.setIntentInterceptor(intentInterceptor);
                        browser.registerTabListCallback(tabListCallback);

                        NavigateParams.Builder navigateParamsBuilder = new NavigateParams.Builder();
                        navigateParamsBuilder.allowIntentLaunchesInBackground();
                        browser.getActiveTab().getNavigationController().navigate(
                                Uri.parse(INTENT_TO_SELF_URL), navigateParamsBuilder.build());
                    }
                });

        mActivityTestRule.launchShell(new Bundle());

        // The intent should be launched...
        intentInterceptor.waitForIntent();
        Intent intent = intentInterceptor.mLastIntent;
        Assert.assertNotNull(intent);
        Assert.assertEquals(INTENT_TO_SELF_PACKAGE, intent.getPackage());
        Assert.assertEquals(INTENT_TO_SELF_ACTION, intent.getAction());
        Assert.assertEquals(INTENT_TO_SELF_DATA_STRING, intent.getDataString());

        // ...the tab created for the initial navigation should be closed...
        onTabRemovedCallbackHelper.waitForFirst();

        // ...and there should now be no tabs in the browser.
        Browser browser = mActivityTestRule.getActivity().getBrowser();
        int numTabs =
                TestThreadUtils.runOnUiThreadBlocking(() -> { return browser.getTabs().size(); });
        Assert.assertEquals(0, numTabs);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { browser.unregisterTabListCallback(tabListCallback); });
    }

    /**
     * Tests that a direct navigation to an external intent in browser startup in incognito mode is
     * blocked as the browser is not yet attached to the window at the time of the navigation and
     * thus the tab is not visible.
     */
    @Test
    @SmallTest
    public void testExternalIntentWithNoRedirectOnBrowserStartupInIncognitoBlockedByDefault()
            throws Throwable {
        CallbackHelper onNavigationFailedCallbackHelper = new CallbackHelper();
        NavigationCallback navigationCallback = new NavigationCallback() {
            @Override
            public void onNavigationFailed(Navigation navigation) {
                if (navigation.getUri().toString().equals(INTENT_TO_SELF_URL)) {
                    onNavigationFailedCallbackHelper.notifyCalled();
                }
            }
        };

        final IntentInterceptor intentInterceptor = new IntentInterceptor();
        InstrumentationActivity.registerOnCreatedCallback(
                new InstrumentationActivity.OnCreatedCallback() {
                    @Override
                    public void onCreated(Browser browser, InstrumentationActivity activity) {
                        Assert.assertEquals(true, browser.getProfile().isIncognito());
                        activity.setIntentInterceptor(intentInterceptor);
                        browser.getActiveTab().getNavigationController().registerNavigationCallback(
                                navigationCallback);
                        browser.getActiveTab().getNavigationController().navigate(
                                Uri.parse(INTENT_TO_SELF_URL));
                    }
                });

        Bundle extras = new Bundle();
        extras.putBoolean(InstrumentationActivity.EXTRA_IS_INCOGNITO, true);
        mActivityTestRule.launchShell(extras);

        // The navigation should fail...
        onNavigationFailedCallbackHelper.waitForFirst();

        // ...the intent should not have been launched...
        Assert.assertNull(intentInterceptor.mLastIntent);

        // ...and there should be one tab in the browser without any navigations in it.
        Browser browser = mActivityTestRule.getActivity().getBrowser();
        int numTabs =
                TestThreadUtils.runOnUiThreadBlocking(() -> { return browser.getTabs().size(); });
        Assert.assertEquals(1, numTabs);
        int numNavigationsInTab = TestThreadUtils.runOnUiThreadBlocking(() -> {
            return browser.getActiveTab().getNavigationController().getNavigationListSize();
        });
        Assert.assertEquals(0, numNavigationsInTab);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            browser.getActiveTab().getNavigationController().unregisterNavigationCallback(
                    navigationCallback);
        });
    }

    /**
     * Tests that a direct navigation to an external intent in browser startup in incognito mode
     * causes an alert dialog to be shown if the embedder specifies that intent launches in the
     * background are allowed for this navigation, and that it is then launched if the user consents
     * via the dialog.
     */
    @Test
    @SmallTest
    @MinWebLayerVersion(89)
    public void
    testExternalIntentWithNoRedirectInBrowserStartupInIncognitoLaunchedWhenBackgroundLaunchesAllowedAndUserConsents()
            throws Throwable {
        CallbackHelper onTabRemovedCallbackHelper = new CallbackHelper();
        TabListCallback tabListCallback = new TabListCallback() {
            @Override
            public void onTabRemoved(Tab tab) {
                onTabRemovedCallbackHelper.notifyCalled();
            }
        };

        final IntentInterceptor intentInterceptor = new IntentInterceptor();
        InstrumentationActivity.registerOnCreatedCallback(
                new InstrumentationActivity.OnCreatedCallback() {
                    @Override
                    public void onCreated(Browser browser, InstrumentationActivity activity) {
                        Assert.assertEquals(true, browser.getProfile().isIncognito());
                        activity.setIntentInterceptor(intentInterceptor);
                        browser.registerTabListCallback(tabListCallback);

                        NavigateParams.Builder navigateParamsBuilder = new NavigateParams.Builder();
                        navigateParamsBuilder.allowIntentLaunchesInBackground();
                        browser.getActiveTab().getNavigationController().navigate(
                                Uri.parse(INTENT_TO_SELF_URL), navigateParamsBuilder.build());
                    }
                });

        Bundle extras = new Bundle();
        extras.putBoolean(InstrumentationActivity.EXTRA_IS_INCOGNITO, true);
        mActivityTestRule.launchShell(extras);

        // The alert dialog notifying the user that they are about to leave incognito should pop up.
        // Click the AlertDialog positive button (button1) when it does.
        onView(withId(android.R.id.button1))
                .check(matches(withText("Leave")))
                .check(matches(isDisplayed()))
                .perform(click());

        // The intent should be launched...
        intentInterceptor.waitForIntent();
        Intent intent = intentInterceptor.mLastIntent;
        Assert.assertNotNull(intent);
        Assert.assertEquals(INTENT_TO_SELF_PACKAGE, intent.getPackage());
        Assert.assertEquals(INTENT_TO_SELF_ACTION, intent.getAction());
        Assert.assertEquals(INTENT_TO_SELF_DATA_STRING, intent.getDataString());

        // ...the tab created for the initial navigation should be closed...
        onTabRemovedCallbackHelper.waitForFirst();

        // ...and there should now be no tabs in the browser.
        Browser browser = mActivityTestRule.getActivity().getBrowser();
        int numTabs =
                TestThreadUtils.runOnUiThreadBlocking(() -> { return browser.getTabs().size(); });
        Assert.assertEquals(0, numTabs);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { browser.unregisterTabListCallback(tabListCallback); });
    }

    /**
     * Tests that a direct navigation to an external intent in browser startup in incognito mode
     * causes an alert dialog to be shown if the embedder specifies that intent launches in the
     * background are allowed for this navigation, and that it is blocked if the user forbids it via
     * the dialog.
     */
    @Test
    @SmallTest
    @MinWebLayerVersion(92)
    public void
    testExternalIntentWithNoRedirectInBrowserStartupInIncognitoBlockedWhenBackgroundLaunchesAllowedAndUserForbids()
            throws Throwable {
        CallbackHelper onNavigationToIntentFailedCallbackHelper = new CallbackHelper();
        CallbackHelper onNavigationToIntentDataStringStartedCallbackHelper = new CallbackHelper();
        NavigationCallback navigationCallback = new NavigationCallback() {
            @Override
            public void onNavigationStarted(Navigation navigation) {
                // There should be no additional navigations after the initial one.
                Assert.assertEquals(INTENT_TO_DUMMY_ACTIVITY_FOR_SPECIAL_SCHEME_DATA_STRING,
                        navigation.getUri().toString());
            }
            @Override
            public void onNavigationFailed(Navigation navigation) {
                if (navigation.getUri().toString().equals(
                            INTENT_TO_DUMMY_ACTIVITY_FOR_SPECIAL_SCHEME_DATA_STRING)) {
                    onNavigationToIntentFailedCallbackHelper.notifyCalled();
                }
            }
        };

        final IntentInterceptor intentInterceptor = new IntentInterceptor();
        InstrumentationActivity.registerOnCreatedCallback(
                new InstrumentationActivity.OnCreatedCallback() {
                    @Override
                    public void onCreated(Browser browser, InstrumentationActivity activity) {
                        activity.setIntentInterceptor(intentInterceptor);
                        Assert.assertEquals(true, browser.getProfile().isIncognito());
                        browser.getActiveTab().getNavigationController().registerNavigationCallback(
                                navigationCallback);

                        NavigateParams.Builder navigateParamsBuilder = new NavigateParams.Builder();
                        navigateParamsBuilder.allowIntentLaunchesInBackground();
                        browser.getActiveTab().getNavigationController().navigate(
                                Uri.parse(INTENT_TO_DUMMY_ACTIVITY_FOR_SPECIAL_SCHEME_DATA_STRING),
                                navigateParamsBuilder.build());
                    }
                });

        Bundle extras = new Bundle();
        extras.putBoolean(InstrumentationActivity.EXTRA_IS_INCOGNITO, true);
        mActivityTestRule.launchShell(extras);

        // The alert dialog notifying the user that they are about to leave incognito should pop up.
        // Click the AlertDialog negative button (button2) when it does.
        onView(withId(android.R.id.button2))
                .check(matches(withText("Stay")))
                .check(matches(isDisplayed()))
                .perform(click());

        // The navigation should fail...
        onNavigationToIntentFailedCallbackHelper.waitForFirst();

        // ...the intent should not have been launched.
        Assert.assertNull(intentInterceptor.mLastIntent);

        // As there was no fallback Url, there should be zero navigations in the tab.
        Browser browser = mActivityTestRule.getActivity().getBrowser();
        int numNavigationsInTab = TestThreadUtils.runOnUiThreadBlocking(() -> {
            return browser.getActiveTab().getNavigationController().getNavigationListSize();
        });
        Assert.assertEquals(0, numNavigationsInTab);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            browser.getActiveTab().getNavigationController().unregisterNavigationCallback(
                    navigationCallback);
        });
    }

    /**
     * Tests that a direct navigation to an external intent in browser startup in incognito mode
     * with the embedder having set an ExternalIntentInIncognitoCallback instance causes that
     * instance to be notified if the embedder specifies that intent launches in the background are
     * allowed for this navigation, and that the intent is then launched if the embedder calls back
     * that the user has consented.
     */
    @Test
    @SmallTest
    @MinWebLayerVersion(93)
    public void
    testExternalIntentWithNoRedirectInBrowserStartupInIncognitoWithEmbedderPresentingWarningDialogLaunchedWhenBackgroundLaunchesAllowedAndUserConsents()
            throws Throwable {
        CallbackHelper onTabRemovedCallbackHelper = new CallbackHelper();
        TabListCallback tabListCallback = new TabListCallback() {
            @Override
            public void onTabRemoved(Tab tab) {
                onTabRemovedCallbackHelper.notifyCalled();
            }
        };

        final IntentInterceptor intentInterceptor = new IntentInterceptor();
        final ExternalIntentInIncognitoCallbackTestImpl externalIntentCallback =
                new ExternalIntentInIncognitoCallbackTestImpl();
        InstrumentationActivity.registerOnCreatedCallback(
                new InstrumentationActivity.OnCreatedCallback() {
                    @Override
                    public void onCreated(Browser browser, InstrumentationActivity activity) {
                        Assert.assertEquals(true, browser.getProfile().isIncognito());
                        activity.setIntentInterceptor(intentInterceptor);
                        browser.registerTabListCallback(tabListCallback);

                        browser.getActiveTab().setExternalIntentInIncognitoCallback(
                                externalIntentCallback);

                        NavigateParams.Builder navigateParamsBuilder = new NavigateParams.Builder();
                        navigateParamsBuilder.allowIntentLaunchesInBackground();
                        browser.getActiveTab().getNavigationController().navigate(
                                Uri.parse(INTENT_TO_SELF_URL), navigateParamsBuilder.build());
                    }
                });

        Bundle extras = new Bundle();
        extras.putBoolean(InstrumentationActivity.EXTRA_IS_INCOGNITO, true);
        mActivityTestRule.launchShell(extras);

        // The embedder should be invoked to present the warning dialog rather than WebLayer
        // presenting the default warning dialog.
        externalIntentCallback.waitForNotificationOnExternalIntentLaunch();
        onView(withText(android.R.id.button1)).check(doesNotExist());

        // Have the embedder notify the implementation that the user has consented.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            externalIntentCallback.getOnUserDecisionCallback().onResult(
                    new Integer(ExternalIntentInIncognitoUserDecision.ALLOW));
        });

        // The intent should be launched...
        intentInterceptor.waitForIntent();
        Intent intent = intentInterceptor.mLastIntent;
        Assert.assertNotNull(intent);
        Assert.assertEquals(INTENT_TO_SELF_PACKAGE, intent.getPackage());
        Assert.assertEquals(INTENT_TO_SELF_ACTION, intent.getAction());
        Assert.assertEquals(INTENT_TO_SELF_DATA_STRING, intent.getDataString());

        // ...the tab created for the initial navigation should be closed...
        onTabRemovedCallbackHelper.waitForFirst();

        // ...and there should now be no tabs in the browser.
        Browser browser = mActivityTestRule.getActivity().getBrowser();
        int numTabs =
                TestThreadUtils.runOnUiThreadBlocking(() -> { return browser.getTabs().size(); });
        Assert.assertEquals(0, numTabs);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { browser.unregisterTabListCallback(tabListCallback); });
    }

    /**
     * Tests that a direct navigation to an external intent in browser startup in incognito mode
     * with the embedder having set an ExternalIntentInIncognitoCallback instance causes that
     * instance to be notified if the embedder specifies that intent launches in the background are
     * allowed for this navigation, and that the intent is then blocked if the embedder calls back
     * that the user has not consented.
     */
    @Test
    @SmallTest
    @MinWebLayerVersion(93)
    public void
    testExternalIntentWithNoRedirectInBrowserStartupInIncognitoWithEmbedderPresentingWarningDialogBlockedWhenBackgroundLaunchesAllowedAndUserForbids()
            throws Throwable {
        CallbackHelper onNavigationToIntentFailedCallbackHelper = new CallbackHelper();
        NavigationCallback navigationCallback = new NavigationCallback() {
            @Override
            public void onNavigationStarted(Navigation navigation) {
                // There should be no additional navigations after the initial one.
                Assert.assertEquals(INTENT_TO_DUMMY_ACTIVITY_FOR_SPECIAL_SCHEME_DATA_STRING,
                        navigation.getUri().toString());
            }
            @Override
            public void onNavigationFailed(Navigation navigation) {
                if (navigation.getUri().toString().equals(
                            INTENT_TO_DUMMY_ACTIVITY_FOR_SPECIAL_SCHEME_DATA_STRING)) {
                    onNavigationToIntentFailedCallbackHelper.notifyCalled();
                }
            }
        };

        final IntentInterceptor intentInterceptor = new IntentInterceptor();
        final ExternalIntentInIncognitoCallbackTestImpl externalIntentCallback =
                new ExternalIntentInIncognitoCallbackTestImpl();
        InstrumentationActivity.registerOnCreatedCallback(
                new InstrumentationActivity.OnCreatedCallback() {
                    @Override
                    public void onCreated(Browser browser, InstrumentationActivity activity) {
                        activity.setIntentInterceptor(intentInterceptor);
                        Assert.assertEquals(true, browser.getProfile().isIncognito());
                        browser.getActiveTab().setExternalIntentInIncognitoCallback(
                                externalIntentCallback);
                        browser.getActiveTab().getNavigationController().registerNavigationCallback(
                                navigationCallback);

                        NavigateParams.Builder navigateParamsBuilder = new NavigateParams.Builder();
                        navigateParamsBuilder.allowIntentLaunchesInBackground();
                        browser.getActiveTab().getNavigationController().navigate(
                                Uri.parse(INTENT_TO_DUMMY_ACTIVITY_FOR_SPECIAL_SCHEME_DATA_STRING),
                                navigateParamsBuilder.build());
                    }
                });

        Bundle extras = new Bundle();
        extras.putBoolean(InstrumentationActivity.EXTRA_IS_INCOGNITO, true);
        mActivityTestRule.launchShell(extras);

        // The embedder should be invoked to present the warning dialog rather than WebLayer
        // presenting the default warning dialog.
        externalIntentCallback.waitForNotificationOnExternalIntentLaunch();
        onView(withText(android.R.id.button1)).check(doesNotExist());

        // Have the embedder notify the implementation that the user has forbidden the launch.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            externalIntentCallback.getOnUserDecisionCallback().onResult(
                    new Integer(ExternalIntentInIncognitoUserDecision.DENY));
        });

        // The navigation should fail...
        onNavigationToIntentFailedCallbackHelper.waitForFirst();

        // ...the intent should not have been launched.
        Assert.assertNull(intentInterceptor.mLastIntent);

        // As there was no fallback Url, there should be zero navigations in the tab.
        Browser browser = mActivityTestRule.getActivity().getBrowser();
        int numNavigationsInTab = TestThreadUtils.runOnUiThreadBlocking(() -> {
            return browser.getActiveTab().getNavigationController().getNavigationListSize();
        });
        Assert.assertEquals(0, numNavigationsInTab);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            browser.getActiveTab().getNavigationController().unregisterNavigationCallback(
                    navigationCallback);
        });
    }

    /**
     * Tests that a direct navigation to an external intent is launched due to the navigation type
     * being set as from a link with a user gesture.
     */
    @Test
    @SmallTest
    @DisableIf.
    Build(sdk_is_less_than = Build.VERSION_CODES.N, message = "https://crbug.com/1176658")
    public void testExternalIntentWithNoRedirectLaunched() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        Tab tab = mActivityTestRule.getActivity().getTab();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { tab.getNavigationController().navigate(Uri.parse(INTENT_TO_SELF_URL)); });

        intentInterceptor.waitForIntent();

        // The current URL should not have changed, and the intent should have been launched.
        Assert.assertEquals(ABOUT_BLANK_URL, mActivityTestRule.getCurrentDisplayUrl());
        Intent intent = intentInterceptor.mLastIntent;
        Assert.assertNotNull(intent);
        Assert.assertEquals(INTENT_TO_SELF_PACKAGE, intent.getPackage());
        Assert.assertEquals(INTENT_TO_SELF_ACTION, intent.getAction());
        Assert.assertEquals(INTENT_TO_SELF_DATA_STRING, intent.getDataString());
    }

    /**
     * Tests that a direct navigation to an external intent is blocked if the client calls
     * Navigation#disableIntentProcessing() from the onNavigationStarted() callback.
     */
    @Test
    @SmallTest
    @MinWebLayerVersion(97)
    public void
    testExternalIntentWithNoRedirectBlockedIfIntentProcessingDisabledOnNavigationStarted()
            throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        CallbackHelper onNavigationToIntentFailedCallbackHelper = new CallbackHelper();
        NavigationCallback navigationCallback = new NavigationCallback() {
            @Override
            public void onNavigationStarted(Navigation navigation) {
                if (navigation.getUri().toString().equals(INTENT_TO_SELF_URL)) {
                    navigation.disableIntentProcessing();
                }
            }
            @Override
            public void onNavigationFailed(Navigation navigation) {
                if (navigation.getUri().toString().equals(INTENT_TO_SELF_URL)) {
                    onNavigationToIntentFailedCallbackHelper.notifyCalled();
                }
            }
        };

        Tab tab = mActivityTestRule.getActivity().getTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tab.getNavigationController().registerNavigationCallback(navigationCallback);
            tab.getNavigationController().navigate(Uri.parse(INTENT_TO_SELF_URL));
        });

        // The navigation should fail...
        onNavigationToIntentFailedCallbackHelper.waitForFirst();

        // ...the intent should not have been launched.
        Assert.assertNull(intentInterceptor.mLastIntent);

        // As there was no fallback Url, there should be only the initial navigation in the tab.
        Browser browser = mActivityTestRule.getActivity().getBrowser();
        int numNavigationsInTab = TestThreadUtils.runOnUiThreadBlocking(() -> {
            return browser.getActiveTab().getNavigationController().getNavigationListSize();
        });
        Assert.assertEquals(1, numNavigationsInTab);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tab.getNavigationController().unregisterNavigationCallback(navigationCallback);
        });
    }

    /**
     * Tests that a navigation to an external intent after a server redirect is blocked if the
     * client calls Navigation#disableIntentProcessing() from the onNavigationStarted()
     * callback.
     */
    @Test
    @SmallTest
    @MinWebLayerVersion(97)
    public void
    testExternalIntentAfterRedirectBlockedIfIntentProcessingDisabledOnNavigationStarted()
            throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        CallbackHelper onNavigationToIntentFailedCallbackHelper = new CallbackHelper();
        NavigationCallback navigationCallback = new NavigationCallback() {
            @Override
            public void onNavigationStarted(Navigation navigation) {
                if (navigation.getUri().toString().equals(mRedirectToIntentToSelfURL)) {
                    navigation.disableIntentProcessing();
                }
            }
            @Override
            public void onNavigationFailed(Navigation navigation) {
                if (navigation.getUri().toString().equals(INTENT_TO_SELF_URL)) {
                    onNavigationToIntentFailedCallbackHelper.notifyCalled();
                }
            }
        };

        Tab tab = mActivityTestRule.getActivity().getTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tab.getNavigationController().registerNavigationCallback(navigationCallback);
            tab.getNavigationController().navigate(Uri.parse(mRedirectToIntentToSelfURL));
        });

        // The navigation should fail...
        onNavigationToIntentFailedCallbackHelper.waitForFirst();

        // ...the intent should not have been launched.
        Assert.assertNull(intentInterceptor.mLastIntent);

        // As there was no fallback Url, there should be only the initial navigation in the tab.
        Browser browser = mActivityTestRule.getActivity().getBrowser();
        int numNavigationsInTab = TestThreadUtils.runOnUiThreadBlocking(() -> {
            return browser.getActiveTab().getNavigationController().getNavigationListSize();
        });
        Assert.assertEquals(1, numNavigationsInTab);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tab.getNavigationController().unregisterNavigationCallback(navigationCallback);
        });
    }

    /**
     * Tests that external intent-related navigation params are not set on browser navigations.
     */
    @Test
    @SmallTest
    @MinWebLayerVersion(89)
    public void testExternalIntentNavigationParamsNotSetOnBrowserNavigations() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);

        navigateAndCheckExternalIntentParams(mTestServerSiteUrl, EXPECT_NAVIGATION_COMPLETION,
                DOESNT_RESULT_IN_EXTERNAL_INTENT, DOESNT_RESULT_IN_USER_DECIDING_EXTERNAL_INTENT);

        // Navigating to an unresolvable intent with a fallback URL should result in a followup
        // browser navigation to the fallback URL.
        navigateAndCheckExternalIntentParams(mNonResolvableIntentWithFallbackUrl,
                mTestServerSiteUrl, EXPECT_NAVIGATION_COMPLETION, DOESNT_RESULT_IN_EXTERNAL_INTENT,
                DOESNT_RESULT_IN_USER_DECIDING_EXTERNAL_INTENT);
    }

    /**
     * Tests that Navigation#wasIntentLaunched() is correctly set on embedder navigations that
     * resolve to intents.
     */
    @Test
    @SmallTest
    @MinWebLayerVersion(89)
    public void testExternalIntentNavigationParamSetOnNavigationsToIntents() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        navigateAndCheckExternalIntentParams(INTENT_TO_SELF_URL, EXPECT_NAVIGATION_FAILURE,
                RESULTS_IN_EXTERNAL_INTENT, DOESNT_RESULT_IN_USER_DECIDING_EXTERNAL_INTENT);
        navigateAndCheckExternalIntentParams(mIntentToSelfWithFallbackUrl,
                EXPECT_NAVIGATION_FAILURE, RESULTS_IN_EXTERNAL_INTENT,
                DOESNT_RESULT_IN_USER_DECIDING_EXTERNAL_INTENT);
        navigateAndCheckExternalIntentParams(mRedirectToIntentToSelfURL, INTENT_TO_SELF_URL,
                EXPECT_NAVIGATION_FAILURE, RESULTS_IN_EXTERNAL_INTENT,
                DOESNT_RESULT_IN_USER_DECIDING_EXTERNAL_INTENT);
        navigateAndCheckExternalIntentParams(mRedirectToCustomSchemeUrlWithDefaultExternalHandler,
                CUSTOM_SCHEME_URL_WITH_DEFAULT_EXTERNAL_HANDLER, EXPECT_NAVIGATION_FAILURE,
                RESULTS_IN_EXTERNAL_INTENT, DOESNT_RESULT_IN_USER_DECIDING_EXTERNAL_INTENT);

        // A navigation that results in an intent that cannot be launched should still fail, but
        // should not have the wasIntentLaunched() parameter set.
        navigateAndCheckExternalIntentParams(MALFORMED_INTENT_URL, EXPECT_NAVIGATION_FAILURE,
                DOESNT_RESULT_IN_EXTERNAL_INTENT, DOESNT_RESULT_IN_USER_DECIDING_EXTERNAL_INTENT);

        // The presence of a fallback URL should not impact the state in the navigation failure
        // callback for a navigation that results in an unresolvable intent.
        navigateAndCheckExternalIntentParams(mNonResolvableIntentWithFallbackUrl,
                EXPECT_NAVIGATION_FAILURE, DOESNT_RESULT_IN_EXTERNAL_INTENT,
                DOESNT_RESULT_IN_USER_DECIDING_EXTERNAL_INTENT);
    }

    /**
     * Tests that Navigation#isUserDecidingIntentLaunch() is correctly set on embedder navigations
     * that resolve to intents in incognito mode.
     */
    @Test
    @SmallTest
    @MinWebLayerVersion(89)
    public void testUserDecidingExternalIntentNavigationParamSetOnNavigationsToIntentsInIncognito()
            throws Throwable {
        Bundle extras = new Bundle();
        extras.putBoolean(InstrumentationActivity.EXTRA_IS_INCOGNITO, true);
        mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL, extras);

        navigateAndCheckExternalIntentParams(INTENT_TO_SELF_URL, EXPECT_NAVIGATION_FAILURE,
                DOESNT_RESULT_IN_EXTERNAL_INTENT, RESULTS_IN_USER_DECIDING_EXTERNAL_INTENT);
    }

    /**
     * Tests that Navigation#wasIntentLaunched() is correctly set on a navigation to an intent that
     * is initiated via a link click.
     */
    @Test
    @SmallTest
    @MinWebLayerVersion(89)
    public void testExternalIntentNavigationParamSetOnIntentLaunchViaLinkClick() throws Throwable {
        // Set up all the prerequisites.
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        Tab tab = mActivityTestRule.getActivity().getTab();

        CallbackHelper navigationFailureCallbackHelper = new CallbackHelper();
        NavigationCallback navigationCallback = new NavigationCallback() {
            @Override
            public void onNavigationFailed(Navigation navigation) {
                Assert.assertEquals(INTENT_TO_SELF_URL, navigation.getUri().toString());
                Assert.assertEquals(true, navigation.wasIntentLaunched());
                Assert.assertEquals(false, navigation.isUserDecidingIntentLaunch());

                navigationFailureCallbackHelper.notifyCalled();
            }
        };
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tab.getNavigationController().registerNavigationCallback(navigationCallback);
        });

        // Navigate to a URL that has a link to an intent, click on the link, and verify via the
        // callback that the navigation to the intent fails with the expected state set.
        String url = mActivityTestRule.getTestDataURL(LINK_WITH_INTENT_TO_SELF_IN_SAME_TAB_FILE);
        mActivityTestRule.navigateAndWait(url);
        mActivityTestRule.executeScriptSync(
                "document.onclick = function() {document.getElementById('link').click()}",
                true /* useSeparateIsolate */);
        EventUtils.simulateTouchCenterOfView(
                mActivityTestRule.getActivity().getWindow().getDecorView());
        navigationFailureCallbackHelper.waitForFirst();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tab.getNavigationController().unregisterNavigationCallback(navigationCallback);
        });
    }

    /**
     * Tests that a navigation that redirects to an external intent results in the external intent
     * being launched.
     */
    @Test
    @SmallTest
    @DisableIf.
    Build(sdk_is_less_than = Build.VERSION_CODES.N, message = "https://crbug.com/1176658")
    public void testExternalIntentAfterRedirectLaunched() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        Tab tab = mActivityTestRule.getActivity().getTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tab.getNavigationController().navigate(Uri.parse(mRedirectToIntentToSelfURL));
        });

        intentInterceptor.waitForIntent();

        // The current URL should not have changed, and the intent should have been launched.
        Assert.assertEquals(ABOUT_BLANK_URL, mActivityTestRule.getCurrentDisplayUrl());
        Intent intent = intentInterceptor.mLastIntent;
        Assert.assertNotNull(intent);
        Assert.assertEquals(INTENT_TO_SELF_PACKAGE, intent.getPackage());
        Assert.assertEquals(INTENT_TO_SELF_ACTION, intent.getAction());
        Assert.assertEquals(INTENT_TO_SELF_DATA_STRING, intent.getDataString());
    }

    /**
     * Tests that a navigation that redirects to a URL with a special scheme that has a default
     * external handler results in an external intent being launched.
     */
    @Test
    @SmallTest
    public void testRedirectToCustomSchemeUrlWithDefaultExternalHandlerLaunchesIntent()
            throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        Tab tab = mActivityTestRule.getActivity().getTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tab.getNavigationController().navigate(
                    Uri.parse(mRedirectToCustomSchemeUrlWithDefaultExternalHandler));
        });

        intentInterceptor.waitForIntent();

        // The current URL should not have changed, and the intent should have been launched.
        Assert.assertEquals(ABOUT_BLANK_URL, mActivityTestRule.getCurrentDisplayUrl());
        Intent intent = intentInterceptor.mLastIntent;
        Assert.assertNotNull(intent);

        Assert.assertEquals(
                INTENT_TO_DUMMY_ACTIVITY_FOR_SPECIAL_SCHEME_PACKAGE, intent.getPackage());
        Assert.assertEquals(INTENT_TO_DUMMY_ACTIVITY_FOR_SPECIAL_SCHEME_ACTION, intent.getAction());
        Assert.assertEquals(
                INTENT_TO_DUMMY_ACTIVITY_FOR_SPECIAL_SCHEME_DATA_STRING, intent.getDataString());
    }

    /**
     * Tests that clicking on a link that goes to an external intent in the same tab results in the
     * external intent being launched.
     */
    @Test
    @SmallTest
    @DisableIf.
    Build(sdk_is_less_than = Build.VERSION_CODES.N, message = "https://crbug.com/1176658")
    public void testExternalIntentInSameTabLaunchedOnLinkClick() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        String url = mActivityTestRule.getTestDataURL(LINK_WITH_INTENT_TO_SELF_IN_SAME_TAB_FILE);

        mActivityTestRule.navigateAndWait(url);

        mActivityTestRule.executeScriptSync(
                "document.onclick = function() {document.getElementById('link').click()}",
                true /* useSeparateIsolate */);
        EventUtils.simulateTouchCenterOfView(
                mActivityTestRule.getActivity().getWindow().getDecorView());

        intentInterceptor.waitForIntent();

        // The current URL should not have changed, and the intent should have been launched.
        Assert.assertEquals(url, mActivityTestRule.getCurrentDisplayUrl());
        Intent intent = intentInterceptor.mLastIntent;
        Assert.assertNotNull(intent);
        Assert.assertEquals(INTENT_TO_SELF_PACKAGE, intent.getPackage());
        Assert.assertEquals(INTENT_TO_SELF_ACTION, intent.getAction());
        Assert.assertEquals(INTENT_TO_SELF_DATA_STRING, intent.getDataString());
    }

    /**
     * Tests that clicking on a link that goes to an external intent in a new tab results in
     * a new tab being opened whose URL is that of the intent and the intent being launched,
     * followed by the new tab being closed.
     */
    @Test
    @SmallTest
    @DisableIf.
    Build(sdk_is_less_than = Build.VERSION_CODES.N, message = "https://crbug.com/1176658")
    public void testExternalIntentInNewTabLaunchedOnLinkClick() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        String url = mActivityTestRule.getTestDataURL(LINK_WITH_INTENT_TO_SELF_IN_NEW_TAB_FILE);

        mActivityTestRule.navigateAndWait(url);

        // Set up listening for the tab addition and removal that we expect to happen.
        CallbackHelper onTabAddedCallbackHelper = new CallbackHelper();
        CallbackHelper onTabRemovedCallbackHelper = new CallbackHelper();
        TabListCallback tabListCallback = new TabListCallback() {
            @Override
            public void onTabAdded(Tab tab) {
                onTabAddedCallbackHelper.notifyCalled();
            }

            @Override
            public void onTabRemoved(Tab tab) {
                onTabRemovedCallbackHelper.notifyCalled();
            }
        };
        Browser browser = mActivityTestRule.getActivity().getBrowser();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { browser.registerTabListCallback(tabListCallback); });

        // Grab the original tab before it changes.
        Tab originalTab = mActivityTestRule.getActivity().getTab();

        mActivityTestRule.executeScriptSync(
                "document.onclick = function() {document.getElementById('link').click()}",
                true /* useSeparateIsolate */);
        EventUtils.simulateTouchCenterOfView(
                mActivityTestRule.getActivity().getWindow().getDecorView());

        // (1) A new tab should be created...
        onTabAddedCallbackHelper.waitForFirst();

        // (2) The intent should be launched in that tab...
        intentInterceptor.waitForIntent();
        Intent intent = intentInterceptor.mLastIntent;
        Assert.assertNotNull(intent);
        Assert.assertEquals(INTENT_TO_SELF_PACKAGE, intent.getPackage());
        Assert.assertEquals(INTENT_TO_SELF_ACTION, intent.getAction());
        Assert.assertEquals(INTENT_TO_SELF_DATA_STRING, intent.getDataString());

        // (3) And finally the new tab should be closed.
        onTabRemovedCallbackHelper.waitForFirst();

        // Now the original tab should be all that's left in the browser, with the display URL being
        // the original URL.
        int numTabs =
                TestThreadUtils.runOnUiThreadBlocking(() -> { return browser.getTabs().size(); });
        Assert.assertEquals(1, numTabs);
        Assert.assertEquals(mActivityTestRule.getActivity().getTab(), originalTab);
        Assert.assertEquals(url, mActivityTestRule.getCurrentDisplayUrl());

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { browser.unregisterTabListCallback(tabListCallback); });
    }

    /**
     * Tests that a navigation that redirects to an external intent with a fallback URL results in
     * the external intent being launched.
     */
    @Test
    @SmallTest
    @DisableIf.
    Build(sdk_is_less_than = Build.VERSION_CODES.N, message = "https://crbug.com/1176658")
    public void testExternalIntentWithFallbackUrlAfterRedirectLaunched() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        String url = mActivityTestRule.getTestServer().getURL(
                "/server-redirect?" + mIntentToSelfWithFallbackUrl);

        Tab tab = mActivityTestRule.getActivity().getTab();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { tab.getNavigationController().navigate(Uri.parse(url)); });

        intentInterceptor.waitForIntent();

        // The current URL should not have changed, and the intent should have been launched.
        Assert.assertEquals(ABOUT_BLANK_URL, mActivityTestRule.getCurrentDisplayUrl());
        Intent intent = intentInterceptor.mLastIntent;
        Assert.assertNotNull(intent);
        Assert.assertEquals(INTENT_TO_SELF_PACKAGE, intent.getPackage());
        Assert.assertEquals(INTENT_TO_SELF_ACTION, intent.getAction());
        Assert.assertEquals(INTENT_TO_SELF_DATA_STRING, intent.getDataString());
    }

    /**
     * Tests that a navigation that redirects to an external intent that can't be handled results in
     * a failed navigation.
     */
    @Test
    @SmallTest
    public void testNonHandledExternalIntentAfterRedirectBlocked() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        String url = mActivityTestRule.getTestServer().getURL(
                "/server-redirect?" + MALFORMED_INTENT_URL);

        Tab tab = mActivityTestRule.getActivity().getTab();

        // Note that this navigation will not result in a paint.
        NavigationWaiter waiter = new NavigationWaiter(
                MALFORMED_INTENT_URL, tab, /*expectFailure=*/true, /*waitForPaint=*/false);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { tab.getNavigationController().navigate(Uri.parse(url)); });
        waiter.waitForNavigation();

        Assert.assertNull(intentInterceptor.mLastIntent);

        // The current URL should not have changed.
        Assert.assertEquals(ABOUT_BLANK_URL, mActivityTestRule.getCurrentDisplayUrl());
    }

    /**
     * Tests that a navigation that redirects to an external intent that can't be handled but has a
     * fallback URL results in a navigation to the fallback URL.
     */
    @Test
    @SmallTest
    public void testNonHandledExternalIntentWithFallbackUrlAfterRedirectGoesToFallbackUrl()
            throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        String url = mActivityTestRule.getTestServer().getURL(
                "/server-redirect?" + mNonResolvableIntentWithFallbackUrl);

        Tab tab = mActivityTestRule.getActivity().getTab();

        NavigationWaiter waiter = new NavigationWaiter(
                mTestServerSiteUrl, tab, /*expectFailure=*/false, /*waitForPaint=*/true);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { tab.getNavigationController().navigate(Uri.parse(url)); });
        waiter.waitForNavigation();

        Assert.assertNull(intentInterceptor.mLastIntent);

        // The current URL should now be the fallback URL.
        Assert.assertEquals(mTestServerSiteUrl, mActivityTestRule.getCurrentDisplayUrl());
    }

    /**
     * |url| is a URL that redirects to an unhandleable intent but has a fallback URL that redirects
     * to a handleable intent.
     * Tests that a navigation to |url| blocks the handleable intent by policy on chained redirects.
     */
    @Test
    @SmallTest
    @DisabledTest(message = "crbug.com/1329813")
    public void
    testNonHandledExternalIntentWithFallbackUrlThatLaunchesIntentAfterRedirectBlocksFallbackIntent()
            throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        String url = mActivityTestRule.getTestServer().getURL(
                "/server-redirect?" + mNonResolvableIntentWithFallbackUrlThatLaunchesIntent);

        Tab tab = mActivityTestRule.getActivity().getTab();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { tab.getNavigationController().navigate(Uri.parse(url)); });

        NavigationWaiter waiter = new NavigationWaiter(
                INTENT_TO_SELF_URL, tab, /*expectFailure=*/true, /*waitForPaint=*/false);
        waiter.waitForNavigation();

        Assert.assertNull(intentInterceptor.mLastIntent);

        // The current URL should not have changed.
        Assert.assertEquals(ABOUT_BLANK_URL, mActivityTestRule.getCurrentDisplayUrl());
    }

    /**
     * Tests that going to a page that loads an intent that can be handled in onload() results in
     * the external intent being launched due to the navigation being specified as being from a link
     * with a user gesture (if the navigation were specified as being from user typing the intent
     * would be blocked due to Chrome's policy on not launching intents from user-typed navigations
     * without a redirect). Also verifies that WebLayer eliminates the navigation entry that
     * launched the intent, so that the user is back on the original URL (i.e., the URL before that
     * of the page that launched the intent in onload().
     */
    @Test
    @SmallTest
    @DisableIf.
    Build(sdk_is_less_than = Build.VERSION_CODES.N, message = "https://crbug.com/1176658")
    public void testExternalIntentViaOnLoadLaunched() throws Throwable {
        String initialUrl = ABOUT_BLANK_URL;
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(initialUrl);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        String url = mActivityTestRule.getTestDataURL(PAGE_THAT_INTENTS_TO_CHROME_ON_LOAD_FILE);

        Tab tab = mActivityTestRule.getActivity().getTab();

        mActivityTestRule.navigateAndWait(url);

        intentInterceptor.waitForIntent();

        // The intent should have been launched, and the user should now be back on the original
        // URL.
        Assert.assertEquals(initialUrl, mActivityTestRule.getCurrentDisplayUrl());
        Intent intent = intentInterceptor.mLastIntent;
        Assert.assertNotNull(intent);
        Assert.assertEquals(INTENT_TO_SELF_PACKAGE, intent.getPackage());
        Assert.assertEquals(INTENT_TO_SELF_ACTION, intent.getAction());
        Assert.assertEquals(INTENT_TO_SELF_DATA_STRING, intent.getDataString());
    }

    /**
     * Tests the following flow:
     * - The user clicks on a link
     * - This link goes to a page that loads a handleable intent in onload()
     * This flow should result in (a) the external intent being launched rather than blocked,
     * because the initial navigation to the page did not occur via user typing, and (b) WebLayer
     * eliminating the navigation entry that launched the intent, so that the user is back on the
     * original URL (i.e., the URL before they clicked the link).
     */
    @Test
    @SmallTest
    @DisableIf.
    Build(sdk_is_less_than = Build.VERSION_CODES.N, message = "https://crbug.com/1176658")
    public void testUserClicksLinkToPageWithExternalIntentLaunchedViaOnLoad() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        String url =
                mActivityTestRule.getTestDataURL(LINK_TO_PAGE_THAT_INTENTS_TO_CHROME_ON_LOAD_FILE);

        mActivityTestRule.navigateAndWait(url);

        // Clicking on the link on this page should result in a navigation to the page that loads an
        // intent in onLoad(), followed by a launching of that intent.
        Tab tab = mActivityTestRule.getActivity().getTab();
        String finalUrl =
                mActivityTestRule.getTestDataURL(PAGE_THAT_INTENTS_TO_CHROME_ON_LOAD_FILE);
        NavigationWaiter waiter =
                new NavigationWaiter(finalUrl, tab, /*expectFailure=*/false, /*waitForPaint=*/true);

        mActivityTestRule.executeScriptSync(
                "document.onclick = function() {document.getElementById('link').click()}",
                true /* useSeparateIsolate */);
        EventUtils.simulateTouchCenterOfView(
                mActivityTestRule.getActivity().getWindow().getDecorView());

        waiter.waitForNavigation();

        intentInterceptor.waitForIntent();

        // The intent should have been launched, and the user should now be back on the original
        // URL.
        Assert.assertEquals(url, mActivityTestRule.getCurrentDisplayUrl());
        Intent intent = intentInterceptor.mLastIntent;
        Assert.assertNotNull(intent);
        Assert.assertEquals(INTENT_TO_SELF_PACKAGE, intent.getPackage());
        Assert.assertEquals(INTENT_TO_SELF_ACTION, intent.getAction());
        Assert.assertEquals(INTENT_TO_SELF_DATA_STRING, intent.getDataString());
    }

    /**
     * Verifies that disableIntentProcessing() does in fact disable intent processing.
     */
    @Test
    @SmallTest
    public void testDisableIntentProcessing() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        String url = mActivityTestRule.getTestDataURL(PAGE_THAT_INTENTS_TO_CHROME_ON_LOAD_FILE);

        Tab tab = mActivityTestRule.getActivity().getTab();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            NavigateParams.Builder navigateParamsBuilder = new NavigateParams.Builder();
            navigateParamsBuilder.disableIntentProcessing();
            tab.getNavigationController().navigate(Uri.parse(url), navigateParamsBuilder.build());
        });

        NavigationWaiter waiter = new NavigationWaiter(
                INTENT_TO_SELF_URL, tab, /*expectFailure=*/true, /*waitForPaint=*/false);
        waiter.waitForNavigation();

        Assert.assertNull(intentInterceptor.mLastIntent);

        // The current URL should not have changed.
        Assert.assertEquals(url, mActivityTestRule.getCurrentDisplayUrl());
    }

    /**
     * Verifies that for an intent with multiple matching apps that weblayer can handle, we avoid
     * the disambiguation dialog and stay in weblayer.
     */
    @Test
    @SmallTest
    public void testAvoidDisambiguationDialog() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        // The data URL isn't valid and will fail the navigation.
        mActivityTestRule.navigateAndWaitForFailure(SPECIALIZED_DATA_URL);

        Assert.assertNull(intentInterceptor.mLastIntent);
        Assert.assertEquals(SPECIALIZED_DATA_URL, mActivityTestRule.getCurrentDisplayUrl());
    }
}
