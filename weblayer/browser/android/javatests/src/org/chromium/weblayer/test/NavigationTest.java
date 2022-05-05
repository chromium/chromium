// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.SystemClock;
import android.util.Pair;
import android.webkit.WebResourceResponse;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.weblayer.Browser;
import org.chromium.weblayer.LoadError;
import org.chromium.weblayer.NavigateParams;
import org.chromium.weblayer.Navigation;
import org.chromium.weblayer.NavigationCallback;
import org.chromium.weblayer.NavigationController;
import org.chromium.weblayer.NavigationState;
import org.chromium.weblayer.NewTabCallback;
import org.chromium.weblayer.Page;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TabCallback;
import org.chromium.weblayer.TabListCallback;
import org.chromium.weblayer.WebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Example test that just starts the weblayer shell.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
public class NavigationTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    // URLs used for base tests.
    private static final String URL1 = "data:text,foo";
    private static final String URL2 = "data:text,bar";
    private static final String URL3 = "data:text,baz";
    private static final String URL4 = "data:text,bat";
    private static final String ENGLISH_PAGE = "english_page.html";
    private static final String FRENCH_PAGE = "french_page.html";
    private static final String STREAM_URL = "https://doesntreallyexist123.com/bar";
    private static final String STREAM_HTML = "<html>foobar</html>";
    private static final String STREAM_INNER_BODY = "foobar";

    // A URL with a custom scheme/host that is handled by WebLayer Shell.
    private static final String CUSTOM_SCHEME_URL_WITH_DEFAULT_EXTERNAL_HANDLER =
            "weblayer://weblayertest/intent";
    // An intent that sends an url with a custom scheme that is handled by WebLayer Shell.
    private static final String INTENT_TO_CUSTOM_SCHEME_URL =
            "intent://weblayertest/intent#Intent;scheme=weblayer;"
            + "action=android.intent.action.VIEW;end";

    // An IntentInterceptor that simply drops intents to ensure that intent launches don't interfere
    // with running of tests.
    private class IntentInterceptor implements InstrumentationActivity.IntentInterceptor {
        @Override
        public void interceptIntent(Intent intent, int requestCode, Bundle options) {}
    }

    private <E extends Throwable> void assertThrows(Class<E> exceptionType, Runnable runnable) {
        Throwable actualException = null;
        try {
            runnable.run();
        } catch (Throwable e) {
            actualException = e;
        }
        assertNotNull("Exception not thrown", actualException);
        assertEquals(exceptionType, actualException.getClass());
    }

    private class Callback extends NavigationCallback {
        public class NavigationCallbackHelper extends CallbackHelper {
            private Uri mUri;
            private boolean mIsSameDocument;
            private int mHttpStatusCode;
            private Map<String, String> mResponseHeaders;
            private List<Uri> mRedirectChain;
            private @LoadError int mLoadError;
            private @NavigationState int mNavigationState;
            private boolean mIsKnownProtocol;
            private boolean mIsPageInitiatedNavigation;
            private boolean mIsServedFromBackForwardCache;
            private boolean mIsFormSubmission;
            private Uri mReferrer;
            private Page mPage;
            private int mNavigationEntryOffset;
            private boolean mWasFetchedFromCache;

            public void notifyCalled(Navigation navigation) {
                notifyCalled(navigation, false);
            }

            public void notifyCalled(Navigation navigation, boolean getPage) {
                mUri = navigation.getUri();
                mIsSameDocument = navigation.isSameDocument();
                mHttpStatusCode = navigation.getHttpStatusCode();
                mRedirectChain = navigation.getRedirectChain();
                mLoadError = navigation.getLoadError();
                mNavigationState = navigation.getState();
                mIsPageInitiatedNavigation = navigation.isPageInitiated();
                int majorVersion = TestThreadUtils.runOnUiThreadBlockingNoException(
                        () -> WebLayer.getSupportedMajorVersion(mActivityTestRule.getActivity()));
                if (majorVersion >= 89) {
                    mIsKnownProtocol = navigation.isKnownProtocol();
                    mIsServedFromBackForwardCache = navigation.isServedFromBackForwardCache();
                }
                if (majorVersion >= 90) {
                    mIsFormSubmission = navigation.isFormSubmission();
                    mReferrer = navigation.getReferrer();
                    if (getPage) {
                        mPage = navigation.getPage();
                    }
                }
                if (majorVersion >= 91) {
                    mResponseHeaders = navigation.getResponseHeaders();
                }
                if (majorVersion >= 92) {
                    mNavigationEntryOffset = navigation.getNavigationEntryOffset();
                }
                if (majorVersion >= 102) {
                    mWasFetchedFromCache = navigation.wasFetchedFromCache();
                }
                notifyCalled();
            }

            public void assertCalledWith(int currentCallCount, String uri) throws TimeoutException {
                waitForCallback(currentCallCount);
                assertEquals(mUri.toString(), uri);
            }

            public void assertCalledWith(int currentCallCount, String uri, boolean isSameDocument)
                    throws TimeoutException {
                waitForCallback(currentCallCount);
                assertEquals(mUri.toString(), uri);
                assertEquals(mIsSameDocument, isSameDocument);
            }

            public void assertCalledWith(int currentCallCount, List<Uri> redirectChain)
                    throws TimeoutException {
                waitForCallback(currentCallCount);
                assertEquals(mRedirectChain, redirectChain);
            }

            public void assertCalledWith(int currentCallCount, String uri, @LoadError int loadError)
                    throws TimeoutException {
                waitForCallback(currentCallCount);
                assertEquals(mUri.toString(), uri);
                assertEquals(mLoadError, loadError);
            }

            public int getHttpStatusCode() {
                return mHttpStatusCode;
            }

            public Map<String, String> getResponseHeaders() {
                return mResponseHeaders;
            }

            @NavigationState
            public int getNavigationState() {
                return mNavigationState;
            }

            public boolean isKnownProtocol() {
                return mIsKnownProtocol;
            }

            public boolean isServedFromBackForwardCache() {
                return mIsServedFromBackForwardCache;
            }

            public boolean isPageInitiated() {
                return mIsPageInitiatedNavigation;
            }

            public boolean isFormSubmission() {
                return mIsFormSubmission;
            }

            public Uri getReferrer() {
                return mReferrer;
            }

            public Page getPage() {
                return mPage;
            }

            public int getNavigationEntryOffset() {
                return mNavigationEntryOffset;
            }

            public boolean wasFetchedFromCache() {
                return mWasFetchedFromCache;
            }
        }

        public class UriCallbackHelper extends CallbackHelper {
            private Uri mUri;

            public void notifyCalled(Uri uri) {
                mUri = uri;
                notifyCalled();
            }

            public Uri getUri() {
                return mUri;
            }
        }

        public class PageCallbackHelper extends CallbackHelper {
            private Page mPage;

            public void notifyCalled(Page page) {
                mPage = page;
                notifyCalled();
            }

            public Page getPage() {
                return mPage;
            }

            public void assertCalledWith(int currentCallCount, Page page) throws TimeoutException {
                waitForCallback(currentCallCount);
                assertEquals(mPage, page);
            }
        }

        public class NavigationCallbackValueRecorder {
            private List<String> mObservedValues =
                    Collections.synchronizedList(new ArrayList<String>());

            public void recordValue(String parameter) {
                mObservedValues.add(parameter);
            }

            public List<String> getObservedValues() {
                return mObservedValues;
            }

            public void waitUntilValueObserved(String expectation) {
                CriteriaHelper.pollInstrumentationThread(
                        () -> Criteria.checkThat(expectation, Matchers.isIn(mObservedValues)));
            }
        }

        public class FirstContentfulPaintCallbackHelper extends CallbackHelper {
            private long mNavigationStartMillis;
            private long mFirstContentfulPaintMs;

            public void notifyCalled(long navigationStartMillis, long firstContentfulPaintMs) {
                mNavigationStartMillis = navigationStartMillis;
                mFirstContentfulPaintMs = firstContentfulPaintMs;
                notifyCalled();
            }

            public long getNavigationStartMillis() {
                return mNavigationStartMillis;
            }

            public long getFirstContentfulPaintMs() {
                return mFirstContentfulPaintMs;
            }
        }

        public class LargestContentfulPaintCallbackHelper extends CallbackHelper {
            private long mNavigationStartMillis;
            private long mLargestContentfulPaintMs;

            public void notifyCalled(long navigationStartMillis, long largestContentfulPaintMs) {
                mNavigationStartMillis = navigationStartMillis;
                mLargestContentfulPaintMs = largestContentfulPaintMs;
                notifyCalled();
            }

            public long getNavigationStartMillis() {
                return mNavigationStartMillis;
            }

            public long getLargestContentfulPaintMs() {
                return mLargestContentfulPaintMs;
            }
        }

        public class PageLanguageDeterminedCallbackHelper extends CallbackHelper {
            private Page mPage;
            private String mLanguage;

            public void notifyCalled(Page page, String language) {
                mPage = page;
                mLanguage = language;
                notifyCalled();
            }

            public Page getPage() {
                return mPage;
            }

            public String getLanguage() {
                return mLanguage;
            }
        }

        public NavigationCallbackHelper onStartedCallback = new NavigationCallbackHelper();
        public NavigationCallbackHelper onRedirectedCallback = new NavigationCallbackHelper();
        public NavigationCallbackHelper onCompletedCallback = new NavigationCallbackHelper();
        public NavigationCallbackHelper onFailedCallback = new NavigationCallbackHelper();
        public NavigationCallbackValueRecorder loadStateChangedCallback =
                new NavigationCallbackValueRecorder();
        public NavigationCallbackValueRecorder loadProgressChangedCallback =
                new NavigationCallbackValueRecorder();
        public CallbackHelper onFirstContentfulPaintCallback = new CallbackHelper();
        public FirstContentfulPaintCallbackHelper onFirstContentfulPaint2Callback =
                new FirstContentfulPaintCallbackHelper();
        public LargestContentfulPaintCallbackHelper onLargestContentfulPaintCallback =
                new LargestContentfulPaintCallbackHelper();
        public UriCallbackHelper onOldPageNoLongerRenderedCallback = new UriCallbackHelper();
        public PageCallbackHelper onPageDestroyedCallback = new PageCallbackHelper();
        public PageLanguageDeterminedCallbackHelper onPageLanguageDeterminedCallback =
                new PageLanguageDeterminedCallbackHelper();

        @Override
        public void onNavigationStarted(Navigation navigation) {
            onStartedCallback.notifyCalled(navigation);
        }

        @Override
        public void onNavigationRedirected(Navigation navigation) {
            onRedirectedCallback.notifyCalled(navigation);
        }

        @Override
        public void onNavigationCompleted(Navigation navigation) {
            onCompletedCallback.notifyCalled(navigation, true);
        }

        @Override
        public void onNavigationFailed(Navigation navigation) {
            onFailedCallback.notifyCalled(navigation);
        }

        @Override
        public void onFirstContentfulPaint() {
            onFirstContentfulPaintCallback.notifyCalled();
        }

        @Override
        public void onFirstContentfulPaint(
                long navigationStartMillis, long firstContentfulPaintMs) {
            onFirstContentfulPaint2Callback.notifyCalled(
                    navigationStartMillis, firstContentfulPaintMs);
        }

        @Override
        public void onLargestContentfulPaint(
                long navigationStartMillis, long largestContentfulPaintMs) {
            onLargestContentfulPaintCallback.notifyCalled(
                    navigationStartMillis, largestContentfulPaintMs);
        }

        @Override
        public void onOldPageNoLongerRendered(Uri newNavigationUri) {
            onOldPageNoLongerRenderedCallback.notifyCalled(newNavigationUri);
        }

        @Override
        public void onLoadStateChanged(boolean isLoading, boolean shouldShowLoadingUi) {
            loadStateChangedCallback.recordValue(
                    Boolean.toString(isLoading) + " " + Boolean.toString(shouldShowLoadingUi));
        }

        @Override
        public void onLoadProgressChanged(double progress) {
            loadProgressChangedCallback.recordValue(
                    progress == 1 ? "load complete" : "load started");
        }

        @Override
        public void onPageDestroyed(Page page) {
            onPageDestroyedCallback.notifyCalled(page);
        }

        @Override
        public void onPageLanguageDetermined(Page page, String language) {
            onPageLanguageDeterminedCallback.notifyCalled(page, language);
        }
    }

    private final Callback mCallback = new Callback();

    @Test
    @SmallTest
    public void testNavigationEvents() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);

        setNavigationCallback(activity);
        int curStartedCount = mCallback.onStartedCallback.getCallCount();
        int curCompletedCount = mCallback.onCompletedCallback.getCallCount();
        int curOnFirstContentfulPaintCount =
                mCallback.onFirstContentfulPaintCallback.getCallCount();

        mActivityTestRule.navigateAndWait(URL2);

        mCallback.onStartedCallback.assertCalledWith(curStartedCount, URL2);
        mCallback.onCompletedCallback.assertCalledWith(curCompletedCount, URL2);
        mCallback.onFirstContentfulPaintCallback.waitForCallback(curOnFirstContentfulPaintCount);
        assertEquals(mCallback.onCompletedCallback.getHttpStatusCode(), 200);
    }

    @Test
    @SmallTest
    public void testOldPageNoLongerRendered() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        setNavigationCallback(activity);

        int renderedCount = mCallback.onOldPageNoLongerRenderedCallback.getCallCount();
        mActivityTestRule.navigateAndWait(URL2);
        mCallback.onOldPageNoLongerRenderedCallback.waitForCallback(renderedCount);
        assertEquals(Uri.parse(URL2), mCallback.onOldPageNoLongerRenderedCallback.getUri());
    }

    @Test
    @SmallTest
    public void testLoadStateUpdates() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        setNavigationCallback(activity);
        mActivityTestRule.navigateAndWait(URL1);

        /* Wait until the NavigationCallback is notified of load completion. */
        mCallback.loadStateChangedCallback.waitUntilValueObserved("false false");
        mCallback.loadProgressChangedCallback.waitUntilValueObserved("load complete");

        /* Verify that the NavigationCallback was notified of load progress /before/ load
         * completion.
         */
        int finishStateIndex =
                mCallback.loadStateChangedCallback.getObservedValues().indexOf("false false");
        int finishProgressIndex =
                mCallback.loadProgressChangedCallback.getObservedValues().indexOf("load complete");
        int startStateIndex =
                mCallback.loadStateChangedCallback.getObservedValues().lastIndexOf("true true");
        int startProgressIndex =
                mCallback.loadProgressChangedCallback.getObservedValues().lastIndexOf(
                        "load started");

        assertNotEquals(startStateIndex, -1);
        assertNotEquals(startProgressIndex, -1);
        assertNotEquals(finishStateIndex, -1);
        assertNotEquals(finishProgressIndex, -1);

        assertTrue(startStateIndex < finishStateIndex);
        assertTrue(startProgressIndex < finishProgressIndex);
    }

    @Test
    @SmallTest
    public void testReplace() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        setNavigationCallback(activity);

        final NavigateParams params =
                new NavigateParams.Builder().setShouldReplaceCurrentEntry(true).build();
        navigateAndWaitForCompletion(URL2,
                ()
                        -> activity.getTab().getNavigationController().navigate(
                                Uri.parse(URL2), params));
        runOnUiThreadBlocking(() -> {
            NavigationController navigationController = activity.getTab().getNavigationController();
            assertFalse(navigationController.canGoForward());
            assertFalse(navigationController.canGoBack());
            assertEquals(1, navigationController.getNavigationListSize());
        });

        // Verify getter works as expected.
        assertTrue(params.getShouldReplaceCurrentEntry());

        // Verify that a default NavigateParams does not replace.
        final NavigateParams params2 = new NavigateParams();
        navigateAndWaitForCompletion(URL3,
                ()
                        -> activity.getTab().getNavigationController().navigate(
                                Uri.parse(URL3), params2));
        runOnUiThreadBlocking(() -> {
            NavigationController navigationController = activity.getTab().getNavigationController();
            assertFalse(navigationController.canGoForward());
            assertTrue(navigationController.canGoBack());
            assertEquals(2, navigationController.getNavigationListSize());
        });
    }

    @Test
    @SmallTest
    public void testGoBackAndForward() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        setNavigationCallback(activity);

        mActivityTestRule.navigateAndWait(URL2);
        mActivityTestRule.navigateAndWait(URL3);

        NavigationController navigationController =
                runOnUiThreadBlocking(() -> activity.getTab().getNavigationController());

        navigateAndWaitForCompletion(URL2, () -> {
            assertTrue(navigationController.canGoBack());
            navigationController.goBack();
        });

        navigateAndWaitForCompletion(URL1, () -> {
            assertTrue(navigationController.canGoBack());
            navigationController.goBack();
        });

        navigateAndWaitForCompletion(URL2, () -> {
            assertFalse(navigationController.canGoBack());
            assertTrue(navigationController.canGoForward());
            navigationController.goForward();
        });

        navigateAndWaitForCompletion(URL3, () -> {
            assertTrue(navigationController.canGoForward());
            navigationController.goForward();
        });

        runOnUiThreadBlocking(() -> { assertFalse(navigationController.canGoForward()); });
    }

    @Test
    @SmallTest
    public void testGoToIndex() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        setNavigationCallback(activity);

        mActivityTestRule.navigateAndWait(URL2);
        mActivityTestRule.navigateAndWait(URL3);
        mActivityTestRule.navigateAndWait(URL4);

        // Navigate back to the 2nd url.
        assertEquals(URL2, goToIndexAndReturnUrl(activity.getTab(), 1));

        // Navigate forwards to the 4th url.
        assertEquals(URL4, goToIndexAndReturnUrl(activity.getTab(), 3));
    }

    @Test
    @SmallTest
    public void testGetNavigationEntryTitle() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(
                "data:text/html,<head><title>Page A</title></head>");
        setNavigationCallback(activity);

        mActivityTestRule.navigateAndWait("data:text/html,<head><title>Page B</title></head>");
        mActivityTestRule.navigateAndWait("data:text/html,<head><title>Page C</title></head>");

        runOnUiThreadBlocking(() -> {
            NavigationController navigationController = activity.getTab().getNavigationController();
            assertEquals("Page A", navigationController.getNavigationEntryTitle(0));
            assertEquals("Page B", navigationController.getNavigationEntryTitle(1));
            assertEquals("Page C", navigationController.getNavigationEntryTitle(2));
        });
    }

    @Test
    @SmallTest
    public void testSameDocument() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        setNavigationCallback(activity);

        int curCompletedCount = mCallback.onCompletedCallback.getCallCount();

        mActivityTestRule.executeScriptSync(
                "history.pushState(null, '', '#bar');", true /* useSeparateIsolate */);

        mCallback.onCompletedCallback.assertCalledWith(
                curCompletedCount, "data:text,foo#bar", true);
    }

    @Test
    @SmallTest
    public void testReload() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        setNavigationCallback(activity);

        navigateAndWaitForCompletion(
                URL1, () -> { activity.getTab().getNavigationController().reload(); });
    }

    @Test
    @SmallTest
    public void testStop() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        setNavigationCallback(activity);

        int curFailedCount = mCallback.onFailedCallback.getCallCount();

        runOnUiThreadBlocking(() -> {
            NavigationController navigationController = activity.getTab().getNavigationController();
            navigationController.registerNavigationCallback(new NavigationCallback() {
                @Override
                public void onNavigationStarted(Navigation navigation) {
                    navigationController.stop();
                }
            });
            navigationController.navigate(Uri.parse(URL2));
        });

        mCallback.onFailedCallback.assertCalledWith(curFailedCount, URL2);
    }

    @Test
    @SmallTest
    public void testRedirect() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        setNavigationCallback(activity);

        int curRedirectedCount = mCallback.onRedirectedCallback.getCallCount();

        String finalUrl = mActivityTestRule.getTestServer().getURL("/echo");
        String url = mActivityTestRule.getTestServer().getURL("/server-redirect?" + finalUrl);
        navigateAndWaitForCompletion(finalUrl,
                () -> { activity.getTab().getNavigationController().navigate(Uri.parse(url)); });

        mCallback.onRedirectedCallback.assertCalledWith(
                curRedirectedCount, Arrays.asList(Uri.parse(url), Uri.parse(finalUrl)));
    }

    /**
     * This test verifies that calling getPage() from within onNavigationFailed for a
     * navigation that results in an error page returns a non-null Page object, and that an
     * onPageDestroyed() callback is triggered for that page when the user navigates away.
     */
    @MinWebLayerVersion(93)
    @Test
    @SmallTest
    public void testPageCallbacksForNavigationResultingInErrorPage() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        CallbackHelper navigationFailedCallbackHelper = new CallbackHelper();
        CallbackHelper pageDestroyedCallbackHelper = new CallbackHelper();
        final Page[] pageForFailedNavigation = {null};
        runOnUiThreadBlocking(() -> {
            NavigationController navigationController = activity.getTab().getNavigationController();
            navigationController.registerNavigationCallback(new NavigationCallback() {
                @Override
                public void onNavigationFailed(Navigation navigation) {
                    assertTrue(navigation.isErrorPage());
                    pageForFailedNavigation[0] = navigation.getPage();
                    assertNotNull(pageForFailedNavigation[0]);
                    navigationFailedCallbackHelper.notifyCalled();
                }
                @Override
                public void onPageDestroyed(Page page) {
                    assertEquals(pageForFailedNavigation[0], page);
                    navigationController.unregisterNavigationCallback(this);
                    pageDestroyedCallbackHelper.notifyCalled();
                }
            });
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Do a navigation that will result in an error page.
            activity.getTab().getNavigationController().navigate(
                    Uri.parse("http://localhost:7/non_existent"));
        });
        navigationFailedCallbackHelper.waitForFirst();

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { activity.getTab().getNavigationController().navigate(Uri.parse(URL1)); });
        pageDestroyedCallbackHelper.waitForFirst();
    }

    /**
     * This is a regression test for crbug.com/1233480, adapted for a change in
     * //content to have such navigations commit rather than fail. It also
     * should not crash nor throw an exception.
     */
    @MinWebLayerVersion(98)
    @Test
    @SmallTest
    @CommandLineFlags.Add("enable-features=" + BlinkFeatures.INITIAL_NAVIGATION_ENTRY)
    public void testInitialRendererInitiatedNavigationToAboutBlankSucceeds() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);

        // Setup a callback for when the navigation in a new tab fails.
        CallbackHelper callbackHelper = new CallbackHelper();
        NewTabCallback newTabCallback = new NewTabCallback() {
            @Override
            public void onNewTab(Tab tab, int mode) {
                NavigationController navigationController = tab.getNavigationController();
                navigationController.registerNavigationCallback(new NavigationCallback() {
                    @Override
                    public void onNavigationCompleted(Navigation navigation) {
                        assertEquals(NavigationState.COMPLETE, navigation.getState());
                        // There should be a valid page for this navigation.
                        assertNotNull(navigation.getPage());
                        navigationController.unregisterNavigationCallback(this);
                        callbackHelper.notifyCalled();
                    }
                });
            }
        };
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { activity.getBrowser().getActiveTab().setNewTabCallback(newTabCallback); });

        // Click on the document to invoke window.open(), which results in a renderer-initiated
        // navigation to about:blank in a new tab.
        mActivityTestRule.executeScriptSync(
                "document.onclick = () => window.open();", true /* useSeparateIsolate */);
        EventUtils.simulateTouchCenterOfView(activity.getWindow().getDecorView());

        callbackHelper.waitForFirst();
    }

    @Test
    @SmallTest
    public void testNavigationList() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        setNavigationCallback(activity);

        mActivityTestRule.navigateAndWait(URL2);
        mActivityTestRule.navigateAndWait(URL3);

        NavigationController navigationController =
                runOnUiThreadBlocking(() -> activity.getTab().getNavigationController());

        runOnUiThreadBlocking(() -> {
            assertEquals(3, navigationController.getNavigationListSize());
            assertEquals(2, navigationController.getNavigationListCurrentIndex());
            assertEquals(URL1, navigationController.getNavigationEntryDisplayUri(0).toString());
            assertEquals(URL2, navigationController.getNavigationEntryDisplayUri(1).toString());
            assertEquals(URL3, navigationController.getNavigationEntryDisplayUri(2).toString());
        });

        navigateAndWaitForCompletion(URL2, () -> { navigationController.goBack(); });

        runOnUiThreadBlocking(() -> {
            assertEquals(3, navigationController.getNavigationListSize());
            assertEquals(1, navigationController.getNavigationListCurrentIndex());
        });
    }

    @Test
    @SmallTest
    public void testLoadError() throws Exception {
        String url = mActivityTestRule.getTestDataURL("non_empty404.html");

        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        setNavigationCallback(activity);

        int curCompletedCount = mCallback.onCompletedCallback.getCallCount();

        // navigateAndWait() expects a success code, so it won't work here.
        runOnUiThreadBlocking(
                () -> { activity.getTab().getNavigationController().navigate(Uri.parse(url)); });

        mCallback.onCompletedCallback.assertCalledWith(
                curCompletedCount, url, LoadError.HTTP_CLIENT_ERROR);
        assertEquals(mCallback.onCompletedCallback.getHttpStatusCode(), 404);
        assertEquals(mCallback.onCompletedCallback.getNavigationState(), NavigationState.COMPLETE);
    }

    @MinWebLayerVersion(89)
    @Test
    @SmallTest
    public void testIsKnownProtocol() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        IntentInterceptor intentInterceptor = new IntentInterceptor();
        activity.setIntentInterceptor(intentInterceptor);
        setNavigationCallback(activity);

        // Test various known protocol cases.
        String httpUrl = mActivityTestRule.getTestDataURL("simple_page.html");
        mActivityTestRule.navigateAndWait(httpUrl);
        assertEquals(true, mCallback.onStartedCallback.isKnownProtocol());
        assertEquals(true, mCallback.onCompletedCallback.isKnownProtocol());

        mActivityTestRule.navigateAndWait("about:blank");
        assertEquals(true, mCallback.onStartedCallback.isKnownProtocol());
        assertEquals(true, mCallback.onCompletedCallback.isKnownProtocol());

        String dataUrl = "data:text,foo";
        mActivityTestRule.navigateAndWait(dataUrl);
        assertEquals(true, mCallback.onStartedCallback.isKnownProtocol());
        assertEquals(true, mCallback.onCompletedCallback.isKnownProtocol());

        // Test external protocol cases.
        mActivityTestRule.navigateAndWaitForFailure(activity.getTab(), INTENT_TO_CUSTOM_SCHEME_URL,
                /*waitForPaint=*/false);
        assertEquals(false, mCallback.onStartedCallback.isKnownProtocol());
        assertEquals(false, mCallback.onFailedCallback.isKnownProtocol());

        mActivityTestRule.navigateAndWaitForFailure(activity.getTab(),
                CUSTOM_SCHEME_URL_WITH_DEFAULT_EXTERNAL_HANDLER,
                /*waitForPaint=*/false);
        assertEquals(false, mCallback.onStartedCallback.isKnownProtocol());
        assertEquals(false, mCallback.onFailedCallback.isKnownProtocol());
    }

    @Test
    @SmallTest
    public void testRepostConfirmation() throws Exception {
        // Load a page with a form.
        InstrumentationActivity activity =
                mActivityTestRule.launchShellWithUrl(mActivityTestRule.getTestDataURL("form.html"));
        assertNotNull(activity);
        setNavigationCallback(activity);

        // Touch the page; this should submit the form.
        int currentCallCount = mCallback.onCompletedCallback.getCallCount();
        EventUtils.simulateTouchCenterOfView(activity.getWindow().getDecorView());
        String targetUrl = mActivityTestRule.getTestDataURL("simple_page.html");
        mCallback.onCompletedCallback.assertCalledWith(currentCallCount, targetUrl);

        // Make sure a tab modal shows after we attempt a reload.
        Boolean isTabModalShowingResult[] = new Boolean[1];
        CallbackHelper callbackHelper = new CallbackHelper();
        runOnUiThreadBlocking(() -> {
            Tab tab = activity.getTab();
            TabCallback callback = new TabCallback() {
                @Override
                public void onTabModalStateChanged(boolean isTabModalShowing) {
                    isTabModalShowingResult[0] = isTabModalShowing;
                    callbackHelper.notifyCalled();
                }
            };
            tab.registerTabCallback(callback);
            tab.getNavigationController().reload();
        });

        callbackHelper.waitForFirst();
        assertTrue(isTabModalShowingResult[0]);
    }

    private void setNavigationCallback(InstrumentationActivity activity) {
        runOnUiThreadBlocking(
                ()
                        -> activity.getTab().getNavigationController().registerNavigationCallback(
                                mCallback));
    }

    private void registerNavigationCallback(NavigationCallback callback) {
        runOnUiThreadBlocking(()
                                      -> mActivityTestRule.getActivity()
                                                 .getTab()
                                                 .getNavigationController()
                                                 .registerNavigationCallback(callback));
    }

    private void unregisterNavigationCallback(NavigationCallback callback) {
        runOnUiThreadBlocking(()
                                      -> mActivityTestRule.getActivity()
                                                 .getTab()
                                                 .getNavigationController()
                                                 .unregisterNavigationCallback(callback));
    }

    private void navigateAndWaitForCompletion(String expectedUrl, Runnable navigateRunnable)
            throws Exception {
        int currentCallCount = mCallback.onCompletedCallback.getCallCount();
        runOnUiThreadBlocking(navigateRunnable);
        mCallback.onCompletedCallback.assertCalledWith(currentCallCount, expectedUrl);
    }

    private String goToIndexAndReturnUrl(Tab tab, int index) throws Exception {
        NavigationController navigationController =
                runOnUiThreadBlocking(() -> tab.getNavigationController());

        final BoundedCountDownLatch navigationComplete = new BoundedCountDownLatch(1);
        final AtomicReference<String> navigationUrl = new AtomicReference<String>();
        NavigationCallback navigationCallback = new NavigationCallback() {
            @Override
            public void onNavigationCompleted(Navigation navigation) {
                navigationComplete.countDown();
                navigationUrl.set(navigation.getUri().toString());
            }
        };

        runOnUiThreadBlocking(() -> {
            navigationController.registerNavigationCallback(navigationCallback);
            navigationController.goToIndex(index);
        });

        navigationComplete.timedAwait();

        runOnUiThreadBlocking(
                () -> { navigationController.unregisterNavigationCallback(navigationCallback); });

        return navigationUrl.get();
    }

    @Test
    @SmallTest
    public void testStopFromOnNavigationStarted() throws Exception {
        final InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        final BoundedCountDownLatch doneLatch = new BoundedCountDownLatch(1);
        NavigationCallback navigationCallback = new NavigationCallback() {
            @Override
            public void onNavigationStarted(Navigation navigation) {
                activity.getTab().getNavigationController().stop();
                doneLatch.countDown();
            }
        };
        runOnUiThreadBlocking(() -> {
            NavigationController controller = activity.getTab().getNavigationController();
            controller.registerNavigationCallback(navigationCallback);
            controller.navigate(Uri.parse(URL1));
        });
        doneLatch.timedAwait();
    }

    // NavigationCallback implementation that sets a header in either start or redirect.
    private static final class HeaderSetter extends NavigationCallback {
        private final String mName;
        private final String mValue;
        private final boolean mInStart;
        public boolean mGotIllegalArgumentException;

        HeaderSetter(String name, String value, boolean inStart) {
            mName = name;
            mValue = value;
            mInStart = inStart;
        }

        @Override
        public void onNavigationStarted(Navigation navigation) {
            if (mInStart) applyHeader(navigation);
        }

        @Override
        public void onNavigationRedirected(Navigation navigation) {
            if (!mInStart) applyHeader(navigation);
        }

        private void applyHeader(Navigation navigation) {
            try {
                navigation.setRequestHeader(mName, mValue);
            } catch (IllegalArgumentException e) {
                mGotIllegalArgumentException = true;
            }
        }
    }

    @Test
    @SmallTest
    public void testSetRequestHeaderInStart() throws Exception {
        TestWebServer testServer = TestWebServer.start();
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        String headerName = "header";
        String headerValue = "value";
        HeaderSetter setter = new HeaderSetter(headerName, headerValue, true);
        registerNavigationCallback(setter);
        String url = testServer.setResponse("/ok.html", "<html>ok</html>", null);
        mActivityTestRule.navigateAndWait(url);
        assertFalse(setter.mGotIllegalArgumentException);
        assertEquals(headerValue, testServer.getLastRequest("/ok.html").headerValue(headerName));
    }

    @Test
    @SmallTest
    public void testSetRequestHeaderInRedirect() throws Exception {
        TestWebServer testServer = TestWebServer.start();
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        String headerName = "header";
        String headerValue = "value";
        HeaderSetter setter = new HeaderSetter(headerName, headerValue, false);
        registerNavigationCallback(setter);
        // The destination of the redirect.
        String finalUrl = testServer.setResponse("/ok.html", "<html>ok</html>", null);
        // The url that redirects to |finalUrl|.
        String redirectingUrl = testServer.setRedirect("/redirect.html", finalUrl);
        Tab tab = mActivityTestRule.getActivity().getTab();
        NavigationWaiter waiter = new NavigationWaiter(finalUrl, tab, false, false);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { tab.getNavigationController().navigate(Uri.parse(redirectingUrl)); });
        waiter.waitForNavigation();
        assertFalse(setter.mGotIllegalArgumentException);
        assertEquals(headerValue, testServer.getLastRequest("/ok.html").headerValue(headerName));
    }

    @Test
    @SmallTest
    public void testSetRequestHeaderThrowsExceptionInCompleted() throws Exception {
        mActivityTestRule.launchShellWithUrl(null);
        boolean gotCompleted[] = new boolean[1];
        NavigationCallback navigationCallback = new NavigationCallback() {
            @Override
            public void onNavigationCompleted(Navigation navigation) {
                gotCompleted[0] = true;
                boolean gotException = false;
                try {
                    navigation.setRequestHeader("name", "value");
                } catch (IllegalStateException e) {
                    gotException = true;
                }
                assertTrue(gotException);
            }
        };
        registerNavigationCallback(navigationCallback);
        mActivityTestRule.navigateAndWait(URL1);
        assertTrue(gotCompleted[0]);
    }

    @Test
    @SmallTest
    public void testSetRequestHeaderThrowsExceptionWithInvalidValue() throws Exception {
        mActivityTestRule.launchShellWithUrl(null);
        HeaderSetter setter = new HeaderSetter("name", "\0", true);
        registerNavigationCallback(setter);
        mActivityTestRule.navigateAndWait(URL1);
        assertTrue(setter.mGotIllegalArgumentException);
    }

    // NavigationCallback implementation that sets the user-agent string in onNavigationStarted().
    private static final class UserAgentSetter extends NavigationCallback {
        private final String mValue;
        public boolean mGotIllegalStateException;

        UserAgentSetter(String value) {
            mValue = value;
        }

        @Override
        public void onNavigationStarted(Navigation navigation) {
            try {
                navigation.setUserAgentString(mValue);
            } catch (IllegalStateException e) {
                mGotIllegalStateException = true;
            }
        }
    }

    @Test
    @SmallTest
    public void testSetUserAgentString() throws Exception {
        TestWebServer testServer = TestWebServer.start();
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        String customUserAgent = "custom-ua";
        UserAgentSetter setter = new UserAgentSetter(customUserAgent);
        registerNavigationCallback(setter);
        String url = testServer.setResponse("/ok.html", "<html>ok</html>", null);
        mActivityTestRule.navigateAndWait(url);
        String actualUserAgent = testServer.getLastRequest("/ok.html").headerValue("User-Agent");
        assertEquals(customUserAgent, actualUserAgent);
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(88)
    public void testCantUsePerNavigationAndDesktopMode() throws Exception {
        TestWebServer testServer = TestWebServer.start();
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        UserAgentSetter setter = new UserAgentSetter("foo");
        registerNavigationCallback(setter);
        String url = testServer.setResponse("/ok.html", "<html>ok</html>", null);
        runOnUiThreadBlocking(() -> { activity.getTab().setDesktopUserAgentEnabled(true); });
        mActivityTestRule.navigateAndWait(url);
        assertTrue(setter.mGotIllegalStateException);
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(88)
    public void testDesktopMode() throws Exception {
        TestWebServer testServer = TestWebServer.start();
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        String url = testServer.setResponse("/ok.html", "<html>ok</html>", null);
        runOnUiThreadBlocking(() -> { activity.getTab().setDesktopUserAgentEnabled(true); });
        mActivityTestRule.navigateAndWait(url);
        String actualUserAgent = testServer.getLastRequest("/ok.html").headerValue("User-Agent");
        assertFalse(actualUserAgent.contains("Android"));
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(88)
    public void testDesktopModeSticks() throws Exception {
        TestWebServer testServer = TestWebServer.start();
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl("about:blank");
        String url = testServer.setResponse("/ok.html", "<html>ok</html>", null);
        String url2 = testServer.setResponse("/ok2.html", "<html>ok</html>", null);
        runOnUiThreadBlocking(() -> { activity.getTab().setDesktopUserAgentEnabled(true); });
        mActivityTestRule.navigateAndWait(url);
        mActivityTestRule.navigateAndWait(url2);
        String actualUserAgent = testServer.getLastRequest("/ok2.html").headerValue("User-Agent");
        assertFalse(actualUserAgent.contains("Android"));
    }

    @Test
    @SmallTest
    @MinWebLayerVersion(88)
    public void testDesktopModeGetter() throws Exception {
        TestWebServer testServer = TestWebServer.start();
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        setNavigationCallback(activity);

        UserAgentSetter setter = new UserAgentSetter("foo");
        registerNavigationCallback(setter);
        mActivityTestRule.navigateAndWait(URL1);
        unregisterNavigationCallback(setter);
        runOnUiThreadBlocking(
                () -> { assertFalse(activity.getTab().isDesktopUserAgentEnabled()); });

        runOnUiThreadBlocking(() -> { activity.getTab().setDesktopUserAgentEnabled(true); });
        mActivityTestRule.navigateAndWait(URL2);
        runOnUiThreadBlocking(() -> { assertTrue(activity.getTab().isDesktopUserAgentEnabled()); });

        navigateAndWaitForCompletion(
                URL1, () -> activity.getTab().getNavigationController().goBack());
        runOnUiThreadBlocking(
                () -> { assertFalse(activity.getTab().isDesktopUserAgentEnabled()); });
    }

    @Test
    @SmallTest
    public void testSkippedNavigationEntry() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        setNavigationCallback(activity);

        int curCompletedCount = mCallback.onCompletedCallback.getCallCount();
        mActivityTestRule.executeScriptSync(
                "history.pushState(null, '', '#foo');", true /* useSeparateIsolate */);
        mCallback.onCompletedCallback.assertCalledWith(curCompletedCount, URL1 + "#foo", true);

        curCompletedCount = mCallback.onCompletedCallback.getCallCount();
        mActivityTestRule.executeScriptSync(
                "history.pushState(null, '', '#bar');", true /* useSeparateIsolate */);
        mCallback.onCompletedCallback.assertCalledWith(curCompletedCount, URL1 + "#bar", true);

        runOnUiThreadBlocking(() -> {
            NavigationController navigationController = activity.getTab().getNavigationController();
            int currentIndex = navigationController.getNavigationListCurrentIndex();
            // Should skip the two previous same document entries, but not the most recent.
            assertFalse(navigationController.isNavigationEntrySkippable(currentIndex));
            assertTrue(navigationController.isNavigationEntrySkippable(currentIndex - 1));
            assertTrue(navigationController.isNavigationEntrySkippable(currentIndex - 2));
        });
    }

    @Test
    @SmallTest
    public void testIndexOutOfBounds() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        runOnUiThreadBlocking(() -> {
            NavigationController controller = activity.getTab().getNavigationController();
            assertIndexOutOfBoundsException(() -> controller.goBack());
            assertIndexOutOfBoundsException(() -> controller.goForward());
            assertIndexOutOfBoundsException(() -> controller.goToIndex(10));
            assertIndexOutOfBoundsException(() -> controller.getNavigationEntryDisplayUri(10));
            assertIndexOutOfBoundsException(() -> controller.getNavigationEntryTitle(10));
            assertIndexOutOfBoundsException(() -> controller.isNavigationEntrySkippable(10));
        });
    }

    private static void assertIndexOutOfBoundsException(Runnable runnable) {
        try {
            runnable.run();
        } catch (IndexOutOfBoundsException e) {
            // Expected exception.
            return;
        }
        Assert.fail("Expected IndexOutOfBoundsException.");
    }

    @Test
    @SmallTest
    public void testPageInitiated() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        setNavigationCallback(activity);
        String initialUrl = mActivityTestRule.getTestDataURL("simple_page4.html");
        mActivityTestRule.navigateAndWait(initialUrl);
        String refreshUrl = mActivityTestRule.getTestDataURL("simple_page.html");
        mCallback.onCompletedCallback.assertCalledWith(
                mCallback.onCompletedCallback.getCallCount(), refreshUrl);
        assertTrue(mCallback.onCompletedCallback.isPageInitiated());
    }

    @Test
    @SmallTest
    public void testPageInitiatedFromClient() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        setNavigationCallback(activity);
        mActivityTestRule.navigateAndWait(URL2);
        assertFalse(mCallback.onStartedCallback.isPageInitiated());
    }

    // Verifies the following sequence doesn't crash:
    // 1. create a new background tab.
    // 2. show modal dialog.
    // 3. destroy tab with modal dialog.
    // 4. switch to background tab created in step 1.
    // This is a regression test for https://crbug.com/1121388.
    @Test
    @SmallTest
    public void testDestroyTabWithModalDialog() throws Exception {
        // Load a page with a form.
        InstrumentationActivity activity =
                mActivityTestRule.launchShellWithUrl(mActivityTestRule.getTestDataURL("form.html"));
        assertNotNull(activity);
        setNavigationCallback(activity);

        // Touch the page; this should submit the form.
        int currentCallCount = mCallback.onCompletedCallback.getCallCount();
        EventUtils.simulateTouchCenterOfView(activity.getWindow().getDecorView());
        String targetUrl = mActivityTestRule.getTestDataURL("simple_page.html");
        mCallback.onCompletedCallback.assertCalledWith(currentCallCount, targetUrl);

        Tab secondTab = runOnUiThreadBlocking(() -> activity.getTab().getBrowser().createTab());
        // Make sure a tab modal shows after we attempt a reload.
        Boolean isTabModalShowingResult[] = new Boolean[1];
        CallbackHelper callbackHelper = new CallbackHelper();
        runOnUiThreadBlocking(() -> {
            Tab tab = activity.getTab();
            Browser browser = tab.getBrowser();
            TabCallback callback = new TabCallback() {
                @Override
                public void onTabModalStateChanged(boolean isTabModalShowing) {
                    tab.unregisterTabCallback(this);
                    isTabModalShowingResult[0] = isTabModalShowing;
                    callbackHelper.notifyCalled();
                }
            };
            tab.registerTabCallback(callback);

            browser.registerTabListCallback(new TabListCallback() {
                @Override
                public void onTabRemoved(Tab tab) {
                    browser.unregisterTabListCallback(this);
                    browser.setActiveTab(secondTab);
                }
            });
            tab.getNavigationController().reload();
        });

        callbackHelper.waitForFirst();
        runOnUiThreadBlocking(() -> {
            Tab tab = activity.getTab();
            tab.getBrowser().destroyTab(tab);
        });
    }

    /**
     * This test verifies calling destroyTab() from within onNavigationFailed doesn't crash.
     */
    @Test
    @SmallTest
    public void testDestroyTabInNavigationFailed() throws Throwable {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        CallbackHelper callbackHelper = new CallbackHelper();
        runOnUiThreadBlocking(() -> {
            NavigationController navigationController = activity.getTab().getNavigationController();
            navigationController.registerNavigationCallback(new NavigationCallback() {
                @Override
                public void onNavigationFailed(Navigation navigation) {
                    navigationController.unregisterNavigationCallback(this);
                    Tab tab = activity.getTab();
                    tab.getBrowser().destroyTab(tab);
                    callbackHelper.notifyCalled();
                }
            });
        });
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            activity.getTab().getNavigationController().navigate(
                    Uri.parse("http://localhost:7/non_existent"));
        });
        callbackHelper.waitForFirst();
    }

    private void navigateToStream(InstrumentationActivity activity, String mimeType,
            String cacheControl, String html) throws Exception {
        int curOnFirstContentfulPaintCount =
                mCallback.onFirstContentfulPaintCallback.getCallCount();
        InputStream stream = new ByteArrayInputStream(html.getBytes(StandardCharsets.UTF_8));
        WebResourceResponse response = new WebResourceResponse(mimeType, "UTF-8", stream);
        if (cacheControl != null) {
            Map<String, String> headers = new HashMap<>();
            headers.put("Cache-Control", cacheControl);
            response.setResponseHeaders(headers);
        }

        final NavigateParams params = new NavigateParams.Builder().setResponse(response).build();
        navigateAndWaitForCompletion(STREAM_URL,
                ()
                        -> activity.getTab().getNavigationController().navigate(
                                Uri.parse(STREAM_URL), params));
        mCallback.onFirstContentfulPaintCallback.waitForCallback(curOnFirstContentfulPaintCount);
    }

    private void navigateToStream(InstrumentationActivity activity, String mimeType,
            String cacheControl) throws Exception {
        navigateToStream(activity, mimeType, cacheControl, STREAM_HTML);
    }

    private void assertStreamContent() throws Exception {
        assertEquals(STREAM_INNER_BODY,
                mActivityTestRule.executeScriptAndExtractString("document.body.innerText"));
    }

    @Test
    @SmallTest
    public void testWebResponse() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        // The code asserts that when InputStreams are used that the stock URL bar is not visible.
        TestThreadUtils.runOnUiThreadBlocking(() -> { activity.getBrowser().setTopView(null); });
        setNavigationCallback(activity);

        navigateToStream(activity, "text/html", null);
        assertStreamContent();
    }

    @Test
    @SmallTest
    public void testWebResponseMimeSniff() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        TestThreadUtils.runOnUiThreadBlocking(() -> { activity.getBrowser().setTopView(null); });
        setNavigationCallback(activity);

        navigateToStream(activity, "", null);
        assertStreamContent();
    }

    @Test
    @SmallTest
    public void testWebResponseNoCacheControl() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        TestThreadUtils.runOnUiThreadBlocking(() -> { activity.getBrowser().setTopView(null); });
        setNavigationCallback(activity);

        navigateToStream(activity, "text/html", null);

        mActivityTestRule.navigateAndWait(URL1);

        int curFailedCount = mCallback.onFailedCallback.getCallCount();
        runOnUiThreadBlocking(() -> { activity.getTab().getNavigationController().goBack(); });
        mCallback.onFailedCallback.assertCalledWith(
                curFailedCount, STREAM_URL, LoadError.CONNECTIVITY_ERROR);
    }

    @Test
    @SmallTest
    public void testWebResponseCached() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        TestThreadUtils.runOnUiThreadBlocking(() -> { activity.getBrowser().setTopView(null); });
        setNavigationCallback(activity);

        navigateToStream(activity, "text/html", "private, max-age=60");

        // Now check that the data can be reused from the cache if it had the correct headers.
        mActivityTestRule.navigateAndWait(URL1);
        int curOnFirstContentfulPaintCount =
                mCallback.onFirstContentfulPaintCallback.getCallCount();
        navigateAndWaitForCompletion(
                STREAM_URL, () -> { activity.getTab().getNavigationController().goBack(); });
        mCallback.onFirstContentfulPaintCallback.waitForCallback(curOnFirstContentfulPaintCount);
        assertStreamContent();
    }

    @Test
    @SmallTest
    public void testWebResponseCachedWithSniffedMimeType() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        TestThreadUtils.runOnUiThreadBlocking(() -> { activity.getBrowser().setTopView(null); });
        setNavigationCallback(activity);

        navigateToStream(activity, "", "private, max-age=60");

        mActivityTestRule.navigateAndWait(URL1);

        int curOnFirstContentfulPaintCount =
                mCallback.onFirstContentfulPaintCallback.getCallCount();
        navigateAndWaitForCompletion(
                STREAM_URL, () -> { activity.getTab().getNavigationController().goBack(); });
        mCallback.onFirstContentfulPaintCallback.waitForCallback(curOnFirstContentfulPaintCount);
        assertStreamContent();
    }

    @DisabledTest(message = "https://crbug.com/1271989")
    @Test
    @SmallTest
    public void testWebResponseNoStore() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        TestThreadUtils.runOnUiThreadBlocking(() -> { activity.getBrowser().setTopView(null); });
        setNavigationCallback(activity);

        navigateToStream(activity, "text/html", "no-store");

        mActivityTestRule.navigateAndWait(URL1);

        int curFailedCount = mCallback.onFailedCallback.getCallCount();
        runOnUiThreadBlocking(() -> { activity.getTab().getNavigationController().goBack(); });
        mCallback.onFailedCallback.assertCalledWith(
                curFailedCount, STREAM_URL, LoadError.CONNECTIVITY_ERROR);
    }

    @DisabledTest(message = "https://crbug.com/1238151")
    @Test
    @SmallTest
    public void testWebResponseExpired() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        TestThreadUtils.runOnUiThreadBlocking(() -> { activity.getBrowser().setTopView(null); });
        setNavigationCallback(activity);

        navigateToStream(activity, "text/html", "private, max-age=2");

        Thread.sleep(5000);

        mActivityTestRule.navigateAndWait(URL1);

        int curFailedCount = mCallback.onFailedCallback.getCallCount();
        runOnUiThreadBlocking(() -> { activity.getTab().getNavigationController().goBack(); });
        mCallback.onFailedCallback.assertCalledWith(
                curFailedCount, STREAM_URL, LoadError.CONNECTIVITY_ERROR);
    }

    // Verifies that a request which uses a stream can still set the user agent that is used for
    // subresources.
    @Test
    @SmallTest
    // The flags are necessary for the following reasons:
    // ignore-certificate-errors: TestWebServer doesn't have a real cert.
    @CommandLineFlags.Add({"ignore-certificate-errors"})
    public void testWebResponseWithUserAgent() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        TestThreadUtils.runOnUiThreadBlocking(() -> { activity.getBrowser().setTopView(null); });
        setNavigationCallback(activity);

        // Avoid mixed http/https errors since the stream url is https.
        TestWebServer testServer = TestWebServer.startSsl();
        String scriptUrl = testServer.setResponse("/foo.js", "", null);
        String streamHtml = "<html><script src='" + scriptUrl + "'/>bar</html>";

        String customUserAgent = "custom-ua";
        UserAgentSetter setter = new UserAgentSetter(customUserAgent);
        registerNavigationCallback(setter);

        navigateToStream(activity, "", null, streamHtml);

        // Ensure the script is fetched.
        CriteriaHelper.pollInstrumentationThread(
                ()
                        -> Criteria.checkThat(
                                testServer.getLastRequest("/foo.js"), Matchers.notNullValue()));

        String actualUserAgent = testServer.getLastRequest("/foo.js").headerValue("User-Agent");
        assertEquals(customUserAgent, actualUserAgent);
    }

    @MinWebLayerVersion(88)
    @Test
    @SmallTest
    public void testOnFirstContentfulPaintTiming() throws Exception {
        long activityStartTimeMs = SystemClock.uptimeMillis();

        TestWebServer testServer = TestWebServer.start();
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        setNavigationCallback(activity);
        String url = testServer.setResponse("/ok.html", "<html>ok</html>", null);

        int count = mCallback.onFirstContentfulPaint2Callback.getCallCount();
        mActivityTestRule.navigateAndWait(url);
        mCallback.onFirstContentfulPaint2Callback.waitForCallback(count);

        long navigationStart = mCallback.onFirstContentfulPaint2Callback.getNavigationStartMillis();
        long current = SystemClock.uptimeMillis();
        Assert.assertTrue(navigationStart <= current);
        Assert.assertTrue(navigationStart >= activityStartTimeMs);

        long firstContentfulPaint =
                mCallback.onFirstContentfulPaint2Callback.getFirstContentfulPaintMs();
        Assert.assertTrue(firstContentfulPaint <= (current - navigationStart));
    }

    @MinWebLayerVersion(88)
    @Test
    @SmallTest
    public void testOnLargestContentfulPaintTiming() throws Exception {
        long activityStartTimeMs = SystemClock.uptimeMillis();

        TestWebServer testServer = TestWebServer.start();
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        setNavigationCallback(activity);
        String url = testServer.setResponse("/ok.html", "<html>ok</html>", null);

        int count = mCallback.onLargestContentfulPaintCallback.getCallCount();
        mActivityTestRule.navigateAndWait(url);

        // Navigate to a new page, as metrics like LCP are only reported at the end of the page load
        // lifetime.
        mActivityTestRule.navigateAndWait("about:blank");
        mCallback.onLargestContentfulPaintCallback.waitForCallback(count);

        long navigationStart =
                mCallback.onLargestContentfulPaintCallback.getNavigationStartMillis();
        long current = SystemClock.uptimeMillis();
        Assert.assertTrue(navigationStart <= current);
        Assert.assertTrue(navigationStart >= activityStartTimeMs);

        long largestContentfulPaint =
                mCallback.onLargestContentfulPaintCallback.getLargestContentfulPaintMs();
        Assert.assertTrue(largestContentfulPaint <= (current - navigationStart));
    }

    /* Disable BackForwardCacheMemoryControls to allow BackForwardCache for all devices regardless
     * of their memory. */
    @MinWebLayerVersion(89)
    @Test
    @SmallTest
    @CommandLineFlags.
    Add({"enable-features=BackForwardCache", "disable-features=BackForwardCacheMemoryControls"})
    public void testServedFromBackForwardCache() throws Exception {
        TestWebServer testServer = TestWebServer.start();
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        setNavigationCallback(activity);

        String url = mActivityTestRule.getTestServer().getURL("/echo");
        navigateAndWaitForCompletion(url,
                () -> { activity.getTab().getNavigationController().navigate(Uri.parse(url)); });
        Assert.assertFalse(mCallback.onStartedCallback.isServedFromBackForwardCache());

        String url2 = testServer.setResponse("/ok.html", "<html>ok</html>", null);
        mActivityTestRule.navigateAndWait(url2);
        Assert.assertFalse(mCallback.onStartedCallback.isServedFromBackForwardCache());

        navigateAndWaitForCompletion(
                url, () -> { activity.getTab().getNavigationController().goBack(); });
        Assert.assertTrue(mCallback.onStartedCallback.isServedFromBackForwardCache());
    }

    @MinWebLayerVersion(90)
    @Test
    @SmallTest
    public void testIsFormSubmission() throws Exception {
        InstrumentationActivity activity =
                mActivityTestRule.launchShellWithUrl(mActivityTestRule.getTestDataURL("form.html"));
        setNavigationCallback(activity);

        // Touch the page; this should submit the form.
        int currentCallCount = mCallback.onStartedCallback.getCallCount();
        EventUtils.simulateTouchCenterOfView(activity.getWindow().getDecorView());

        mCallback.onStartedCallback.waitForCallback(currentCallCount);
        assertEquals(true, mCallback.onStartedCallback.isFormSubmission());
    }

    @MinWebLayerVersion(90)
    @Test
    @SmallTest
    public void testGetReferrer() throws Exception {
        TestWebServer testServer = TestWebServer.start();
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        setNavigationCallback(activity);
        String referrer = "http://foo.com/";
        NavigationCallback navigationCallback = new NavigationCallback() {
            @Override
            public void onNavigationStarted(Navigation navigation) {
                try {
                    navigation.setRequestHeader("Referer", referrer);
                } catch (IllegalStateException e) {
                }
            }
        };

        registerNavigationCallback(navigationCallback);
        int currentCallCount = mCallback.onCompletedCallback.getCallCount();
        String url = testServer.setResponse("/ok.html", "<html>ok</html>", null);
        mActivityTestRule.navigateAndWait(url);
        mCallback.onCompletedCallback.waitForCallback(currentCallCount);
        assertEquals(referrer, mCallback.onCompletedCallback.getReferrer().toString());
    }

    /* Disable BackForwardCacheMemoryControls to allow BackForwardCache for all devices regardless
     * of their memory. */
    @MinWebLayerVersion(90)
    @Test
    @SmallTest
    @CommandLineFlags.
    Add({"enable-features=BackForwardCache", "disable-features=BackForwardCacheMemoryControls"})
    public void testPageApi() throws Exception {
        TestWebServer testServer = TestWebServer.start();
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        setNavigationCallback(activity);

        String url1 = mActivityTestRule.getTestServer().getURL("/echo");
        navigateAndWaitForCompletion(url1,
                () -> { activity.getTab().getNavigationController().navigate(Uri.parse(url1)); });
        Page page1 = mCallback.onCompletedCallback.getPage();

        // Ensure the second page doesn't go into bfcache so that we can observe its Page object
        // being destroyed.
        List<Pair<String, String>> headers =
                Collections.singletonList(Pair.create("Cache-Control", "no-store"));
        String url2 = testServer.setResponse("/ok.html", "<html>ok</html>", headers);
        mActivityTestRule.navigateAndWait(url2);
        Page page2 = mCallback.onCompletedCallback.getPage();
        assertNotEquals(page1, page2);

        int curOnPageDestroyedCount = mCallback.onPageDestroyedCallback.getCallCount();

        navigateAndWaitForCompletion(
                url1, () -> { activity.getTab().getNavigationController().goBack(); });
        Assert.assertTrue(mCallback.onCompletedCallback.isServedFromBackForwardCache());
        Page page3 = mCallback.onCompletedCallback.getPage();
        assertEquals(page1, page3);

        mCallback.onPageDestroyedCallback.assertCalledWith(curOnPageDestroyedCount, page2);
    }

    @MinWebLayerVersion(91)
    @Test
    @SmallTest
    public void testResponseHeaders() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        setNavigationCallback(activity);

        int curCompletedCount = mCallback.onCompletedCallback.getCallCount();

        String url = mActivityTestRule.getTestServer().getURL("/echo");
        mActivityTestRule.navigateAndWait(url);

        mCallback.onCompletedCallback.assertCalledWith(curCompletedCount, url);

        Map<String, String> headers = mCallback.onCompletedCallback.getResponseHeaders();
        assertEquals(headers.get("Content-Type"), "text/html");
    }

    @MinWebLayerVersion(92)
    @Test
    @SmallTest
    public void testGetNavigationEntryOffset() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        setNavigationCallback(activity);

        mActivityTestRule.navigateAndWait(URL2);
        assertEquals(1, mCallback.onCompletedCallback.getNavigationEntryOffset());

        mActivityTestRule.navigateAndWait(URL3);
        assertEquals(1, mCallback.onCompletedCallback.getNavigationEntryOffset());

        NavigationController navigationController =
                runOnUiThreadBlocking(() -> activity.getTab().getNavigationController());

        navigateAndWaitForCompletion(URL2, () -> navigationController.goBack());
        assertEquals(-1, mCallback.onCompletedCallback.getNavigationEntryOffset());

        navigateAndWaitForCompletion(URL3, () -> navigationController.goForward());
        assertEquals(1, mCallback.onCompletedCallback.getNavigationEntryOffset());

        navigateAndWaitForCompletion(URL3, () -> navigationController.reload());
        assertEquals(0, mCallback.onCompletedCallback.getNavigationEntryOffset());

        navigateAndWaitForCompletion(URL1, () -> navigationController.goToIndex(0));
        assertEquals(-2, mCallback.onCompletedCallback.getNavigationEntryOffset());

        navigateAndWaitForCompletion(URL3, () -> navigationController.goToIndex(2));
        assertEquals(2, mCallback.onCompletedCallback.getNavigationEntryOffset());
    }

    @MinWebLayerVersion(93)
    @Test
    @SmallTest
    public void testOnPageLanguageDetermined() throws Exception {
        TestWebServer testServer = TestWebServer.start();
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(null);
        setNavigationCallback(activity);

        int curLanguageDeterminedCount = mCallback.onPageLanguageDeterminedCallback.getCallCount();

        // Navigate to a page in English.
        String url = mActivityTestRule.getTestDataURL(ENGLISH_PAGE);
        mActivityTestRule.navigateAndWait(url);

        Page committedPage = mCallback.onCompletedCallback.getPage();
        assertNotNull(committedPage);

        // Verify that the language determined callback is fired as expected.
        mCallback.onPageLanguageDeterminedCallback.waitForCallback(curLanguageDeterminedCount);

        assertEquals(committedPage, mCallback.onPageLanguageDeterminedCallback.getPage());
        assertEquals("en", mCallback.onPageLanguageDeterminedCallback.getLanguage());

        // Now navigate to a page in French.
        committedPage = null;
        curLanguageDeterminedCount = mCallback.onPageLanguageDeterminedCallback.getCallCount();

        url = mActivityTestRule.getTestDataURL(FRENCH_PAGE);
        mActivityTestRule.navigateAndWait(url);

        committedPage = mCallback.onCompletedCallback.getPage();
        assertNotNull(committedPage);

        // Verify that the language determined callback is fired as expected.
        mCallback.onPageLanguageDeterminedCallback.waitForCallback(curLanguageDeterminedCount);

        assertEquals(committedPage, mCallback.onPageLanguageDeterminedCallback.getPage());
        assertEquals("fr", mCallback.onPageLanguageDeterminedCallback.getLanguage());
    }

    @MinWebLayerVersion(102)
    @Test
    @SmallTest
    public void testWasFetchedFromCache() throws Exception {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(URL1);
        setNavigationCallback(activity);
        String url = mActivityTestRule.getTestServer().getURL("/cachetime");

        mActivityTestRule.navigateAndWait(url);
        assertFalse(mCallback.onCompletedCallback.wasFetchedFromCache());

        mActivityTestRule.navigateAndWait(
                mActivityTestRule.getTestServer().getURL("/cachetime?foo"));
        assertFalse(mCallback.onCompletedCallback.wasFetchedFromCache());

        mActivityTestRule.navigateAndWait(url);
        assertTrue(mCallback.onCompletedCallback.wasFetchedFromCache());
    }
}
