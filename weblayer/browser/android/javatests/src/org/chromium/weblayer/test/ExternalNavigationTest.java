// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

import androidx.fragment.app.Fragment;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Browser;
import org.chromium.weblayer.NavigateParams;
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

    /**
     * A dummy activity that claims to handle "weblayer://weblayertest".
     */
    public static class DummyActivityForSpecialScheme extends Activity {
        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            finish();
        }
    }

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
    private static final String INTENT_TO_CHROME_DATA_CONTENT =
            "play.google.com/store/apps/details?id=com.facebook.katana/";
    private static final String INTENT_TO_CHROME_SCHEME = "https";
    private static final String INTENT_TO_CHROME_DATA_STRING =
            INTENT_TO_CHROME_SCHEME + "://" + INTENT_TO_CHROME_DATA_CONTENT;
    private static final String INTENT_TO_CHROME_ACTION = "android.intent.action.VIEW";
    private static final String INTENT_TO_CHROME_PACKAGE = "com.android.chrome";

    // An intent that opens Chrome to view a specified URL. Note that the "end" is left off to allow
    // appending extras when constructing URLs.
    private static final String INTENT_TO_CHROME = "intent://" + INTENT_TO_CHROME_DATA_CONTENT
            + "#Intent;scheme=" + INTENT_TO_CHROME_SCHEME + ";action=" + INTENT_TO_CHROME_ACTION
            + ";package=" + INTENT_TO_CHROME_PACKAGE + ";";
    private static final String INTENT_TO_CHROME_URL = INTENT_TO_CHROME + "end";

    // An intent URL that gets rejected as malformed.
    private static final String MALFORMED_INTENT_URL = "intent://garbage;end";

    // An intent that is properly formed but wishes to open an app that is not present on the
    // device. Note that the "end" is left off to allow appending extras when constructing URLs.
    private static final String NON_RESOLVABLE_INTENT =
            "intent://dummy.com/#Intent;scheme=https;action=android.intent.action.VIEW;package=com.missing.app;";

    private static final String LINK_WITH_INTENT_TO_CHROME_IN_SAME_TAB_FILE =
            "link_with_intent_to_chrome_in_same_tab.html";
    private static final String LINK_WITH_INTENT_TO_CHROME_IN_NEW_TAB_FILE =
            "link_with_intent_to_chrome_in_new_tab.html";
    private static final String PAGE_THAT_INTENTS_TO_CHROME_ON_LOAD_FILE =
            "page_that_intents_to_chrome_on_load.html";
    private static final String LINK_TO_PAGE_THAT_INTENTS_TO_CHROME_ON_LOAD_FILE =
            "link_to_page_that_intents_to_chrome_on_load.html";

    // The test server handles "echo" with a response containing "Echo" :).
    private final String mTestServerSiteUrl = mActivityTestRule.getTestServer().getURL("/echo");

    private final String mTestServerSiteFallbackUrlExtra =
            "S.browser_fallback_url=" + android.net.Uri.encode(mTestServerSiteUrl) + ";";
    private final String mIntentToChromeWithFallbackUrl =
            INTENT_TO_CHROME + mTestServerSiteFallbackUrlExtra + "end";
    private final String mNonResolvableIntentWithFallbackUrl =
            NON_RESOLVABLE_INTENT + mTestServerSiteFallbackUrlExtra + "end";

    private final String mRedirectToCustomSchemeUrlWithDefaultExternalHandler =
            mActivityTestRule.getTestServer().getURL(
                    "/server-redirect?" + CUSTOM_SCHEME_URL_WITH_DEFAULT_EXTERNAL_HANDLER);
    private final String mRedirectToIntentToChromeURL =
            mActivityTestRule.getTestServer().getURL("/server-redirect?" + INTENT_TO_CHROME_URL);
    private final String mNonResolvableIntentWithFallbackUrlThatLaunchesIntent =
            NON_RESOLVABLE_INTENT + "S.browser_fallback_url="
            + android.net.Uri.encode(mRedirectToIntentToChromeURL) + ";end";

    private class IntentInterceptor implements InstrumentationActivity.IntentInterceptor {
        public Intent mLastIntent;
        private CallbackHelper mCallbackHelper = new CallbackHelper();

        @Override
        public void interceptIntent(
                Fragment fragment, Intent intent, int requestCode, Bundle options) {
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
     * Tests that a direct navigation to an external intent is launched due to the navigation type
     * being set as from a link with a user gesture.
     */
    @Test
    @SmallTest
    public void testExternalIntentWithNoRedirectLaunched() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        Tab tab = mActivityTestRule.getActivity().getTab();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { tab.getNavigationController().navigate(Uri.parse(INTENT_TO_CHROME_URL)); });

        intentInterceptor.waitForIntent();

        // The current URL should not have changed, and the intent should have been launched.
        Assert.assertEquals(ABOUT_BLANK_URL, mActivityTestRule.getCurrentDisplayUrl());
        Intent intent = intentInterceptor.mLastIntent;
        Assert.assertNotNull(intent);
        Assert.assertEquals(INTENT_TO_CHROME_PACKAGE, intent.getPackage());
        Assert.assertEquals(INTENT_TO_CHROME_ACTION, intent.getAction());
        Assert.assertEquals(INTENT_TO_CHROME_DATA_STRING, intent.getDataString());
    }

    /**
     * Tests that a navigation that redirects to an external intent results in the external intent
     * being launched.
     */
    @Test
    @SmallTest
    public void testExternalIntentAfterRedirectLaunched() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        Tab tab = mActivityTestRule.getActivity().getTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            tab.getNavigationController().navigate(Uri.parse(mRedirectToIntentToChromeURL));
        });

        intentInterceptor.waitForIntent();

        // The current URL should not have changed, and the intent should have been launched.
        Assert.assertEquals(ABOUT_BLANK_URL, mActivityTestRule.getCurrentDisplayUrl());
        Intent intent = intentInterceptor.mLastIntent;
        Assert.assertNotNull(intent);
        Assert.assertEquals(INTENT_TO_CHROME_PACKAGE, intent.getPackage());
        Assert.assertEquals(INTENT_TO_CHROME_ACTION, intent.getAction());
        Assert.assertEquals(INTENT_TO_CHROME_DATA_STRING, intent.getDataString());
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
    public void testExternalIntentInSameTabLaunchedOnLinkClick() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        String url = mActivityTestRule.getTestDataURL(LINK_WITH_INTENT_TO_CHROME_IN_SAME_TAB_FILE);

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
        Assert.assertEquals(INTENT_TO_CHROME_PACKAGE, intent.getPackage());
        Assert.assertEquals(INTENT_TO_CHROME_ACTION, intent.getAction());
        Assert.assertEquals(INTENT_TO_CHROME_DATA_STRING, intent.getDataString());
    }

    /**
     * Tests that clicking on a link that goes to an external intent in a new tab results in
     * a new tab being opened whose URL is that of the intent and the intent being launched,
     * followed by the new tab being closed.
     */
    @Test
    @SmallTest
    public void testExternalIntentInNewTabLaunchedOnLinkClick() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        String url = mActivityTestRule.getTestDataURL(LINK_WITH_INTENT_TO_CHROME_IN_NEW_TAB_FILE);

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
        Assert.assertEquals(INTENT_TO_CHROME_PACKAGE, intent.getPackage());
        Assert.assertEquals(INTENT_TO_CHROME_ACTION, intent.getAction());
        Assert.assertEquals(INTENT_TO_CHROME_DATA_STRING, intent.getDataString());

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
    public void testExternalIntentWithFallbackUrlAfterRedirectLaunched() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);

        String url = mActivityTestRule.getTestServer().getURL(
                "/server-redirect?" + mIntentToChromeWithFallbackUrl);

        Tab tab = mActivityTestRule.getActivity().getTab();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { tab.getNavigationController().navigate(Uri.parse(url)); });

        intentInterceptor.waitForIntent();

        // The current URL should not have changed, and the intent should have been launched.
        Assert.assertEquals(ABOUT_BLANK_URL, mActivityTestRule.getCurrentDisplayUrl());
        Intent intent = intentInterceptor.mLastIntent;
        Assert.assertNotNull(intent);
        Assert.assertEquals(INTENT_TO_CHROME_PACKAGE, intent.getPackage());
        Assert.assertEquals(INTENT_TO_CHROME_ACTION, intent.getAction());
        Assert.assertEquals(INTENT_TO_CHROME_DATA_STRING, intent.getDataString());
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
                INTENT_TO_CHROME_URL, tab, /*expectFailure=*/true, /*waitForPaint=*/false);
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
        Assert.assertEquals(INTENT_TO_CHROME_PACKAGE, intent.getPackage());
        Assert.assertEquals(INTENT_TO_CHROME_ACTION, intent.getAction());
        Assert.assertEquals(INTENT_TO_CHROME_DATA_STRING, intent.getDataString());
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
        Assert.assertEquals(INTENT_TO_CHROME_PACKAGE, intent.getPackage());
        Assert.assertEquals(INTENT_TO_CHROME_ACTION, intent.getAction());
        Assert.assertEquals(INTENT_TO_CHROME_DATA_STRING, intent.getDataString());
    }

    /**
     * Verifies that disableIntentProcessing() does in fact disable intent processing.
     */
    @Test
    @SmallTest
    @MinWebLayerVersion(87)
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
                INTENT_TO_CHROME_URL, tab, /*expectFailure=*/true, /*waitForPaint=*/false);
        waiter.waitForNavigation();

        Assert.assertNull(intentInterceptor.mLastIntent);

        // The current URL should not have changed.
        Assert.assertEquals(url, mActivityTestRule.getCurrentDisplayUrl());
    }
}
