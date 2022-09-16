// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.os.Bundle;
import android.os.RemoteException;
import android.util.Pair;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.test.util.AnnotationRule;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.ExecutionException;

/**
 * Test class to test UrlBarController logic.
 */
@RunWith(WebLayerJUnit4ClassRunner.class)
@DisabledTest(message = "https://crbug.com/1315403")
public class UrlBarControllerTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private static final String ABOUT_BLANK_URL = "about:blank";
    private static final String NEW_TAB_URL = "new_browser.html";
    private static final String HTTP_SCHEME = "http://";
    private static final String PAGE_WITH_TITLE =
            "<!DOCTYPE html><html><head><title>Example title</title></head></html>";

    private TestWebServer mWebServer;
    // The test server handles "echo" with a response containing "Echo" :).
    private String mTestServerSiteUrl;

    /**
     * Annotation to override the trusted CDN.
     */
    @Retention(RetentionPolicy.RUNTIME)
    private @interface OverrideTrustedCdn {}

    private static class OverrideTrustedCdnRule extends AnnotationRule {
        public OverrideTrustedCdnRule() {
            super(OverrideTrustedCdn.class);
        }

        /**
         * @return Whether the trusted CDN should be overridden.
         */
        public boolean isEnabled() {
            return !getAnnotations().isEmpty();
        }
    }

    @Rule
    public OverrideTrustedCdnRule mOverrideTrustedCdn = new OverrideTrustedCdnRule();

    @Before
    public void setUp() throws Exception {
        mWebServer = TestWebServer.start();
        mTestServerSiteUrl = mActivityTestRule.getTestServer().getURL("/echo");
        if (mOverrideTrustedCdn.isEnabled()) {
            CommandLine.getInstance().appendSwitchWithValue(
                    "trusted-cdn-base-url-for-tests", mWebServer.getBaseUrl());
            mActivityTestRule.writeCommandLineFile();
        }
    }

    @After
    public void tearDown() {
        mWebServer.shutdown();
    }

    private String getDisplayedUrl() {
        try {
            InstrumentationActivity activity = mActivityTestRule.getActivity();
            TestWebLayer testWebLayer =
                    TestWebLayer.getTestWebLayer(activity.getApplicationContext());
            View urlBarView = activity.getUrlBarView();
            return testWebLayer.getDisplayedUrl(urlBarView);
        } catch (RemoteException e) {
            throw new RuntimeException(e);
        }
    }

    private int getIcon(String name) {
        InstrumentationActivity activity = mActivityTestRule.getActivity();
        String packageName =
                TestWebLayer.getWebLayerContext(activity.getApplicationContext()).getPackageName();
        return ResourceUtil.getIdentifier(
                TestWebLayer.getRemoteContext(activity.getApplicationContext()), name, packageName);
    }

    private int getDefaultSecurityIcon() throws RemoteException {
        // On tablets an info icon is shown for ConnectionSecurityLevel.NONE pages,
        // on smaller form factors nothing.
        InstrumentationActivity activity = mActivityTestRule.getActivity();
        TestWebLayer testWebLayer = TestWebLayer.getTestWebLayer(activity.getApplicationContext());
        if (testWebLayer.isWindowOnSmallDevice(mActivityTestRule.getActivity().getBrowser())) {
            return 0;
        }

        return getIcon("drawable/omnibox_info");
    }

    private int getAmpIcon() throws RemoteException {
        return getIcon("drawable/amp_icon");
    }

    private void runTrustedCdnPublisherUrlTest(
            String publisherUrl, String expectedPublisher, int expectedSecurityIcon) {
        final List<Pair<String, String>> headers;
        if (publisherUrl == null) {
            headers = null;
        } else {
            headers = Collections.singletonList(Pair.create("X-AMP-Cache", publisherUrl));
        }
        String testUrl = mWebServer.setResponse("/test.html", PAGE_WITH_TITLE, headers);

        mActivityTestRule.navigateAndWait(testUrl);

        final String expectedUrl;
        if (expectedPublisher == null) {
            // Remove everything but the TLD because these aren't displayed.
            String temp = testUrl.substring(HTTP_SCHEME.length());
            expectedUrl = temp.substring(0, temp.indexOf("/"));
        } else {
            expectedUrl = String.format(Locale.US, "%s – delivered by Google", expectedPublisher);
        }

        Assert.assertEquals(expectedUrl, getDisplayedUrl());

        verifySecurityIcon(expectedSecurityIcon);
    }

    private void verifySecurityIcon(int expectedSecurityIcon) {
        InstrumentationActivity activity = mActivityTestRule.getActivity();
        TestWebLayer testWebLayer = TestWebLayer.getTestWebLayer(activity.getApplicationContext());
        View urlBarView = activity.getUrlBarView();
        try {
            ImageView securityButton = testWebLayer.getSecurityButton(urlBarView);

            if (expectedSecurityIcon == 0) {
                Assert.assertEquals(View.INVISIBLE, securityButton.getVisibility());
                return;
            }

            CriteriaHelper.pollInstrumentationThread(() -> {
                Criteria.checkThat(securityButton.getVisibility(), Matchers.is(View.VISIBLE));
            });
            Bitmap expected = BitmapFactory.decodeResource(
                    TestWebLayer.getRemoteContext(activity.getApplicationContext()).getResources(),
                    expectedSecurityIcon);
            Bitmap shown = ((BitmapDrawable) securityButton.getDrawable()).getBitmap();
            // Below should work but fails, so do it manually.
            // Assert.assertTrue(expected.sameAs(
            //         ((BitmapDrawable) securityButton.getDrawable()).getBitmap()));
            Assert.assertEquals(expected.getWidth(), shown.getWidth());
            Assert.assertEquals(expected.getHeight(), shown.getHeight());
            Assert.assertEquals(expected.getConfig(), shown.getConfig());
            int[] expectedPixels = new int[expected.getWidth() * expected.getHeight()];
            expected.getPixels(expectedPixels, 0, expected.getWidth(), 0, 0, expected.getWidth(),
                    expected.getHeight());
            int[] shownPixels = new int[shown.getWidth() * shown.getHeight()];
            shown.getPixels(
                    shownPixels, 0, shown.getWidth(), 0, 0, shown.getWidth(), shown.getHeight());
            Assert.assertTrue(Arrays.equals(expectedPixels, shownPixels));
        } catch (RemoteException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Tests that UrlBarView can be instantiated and shown.
     */
    @Test
    @SmallTest
    public void testShowUrlBar() throws RemoteException {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        Assert.assertEquals(ABOUT_BLANK_URL, mActivityTestRule.getCurrentDisplayUrl());
    }

    /**
     * Tests that UrlBarView contains an ImageButton and a TextView with the expected text.
     */
    @Test
    @SmallTest
    public void testUrlBarView() throws RemoteException {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        View urlBarView = activity.getUrlBarView();

        Assert.assertEquals(ABOUT_BLANK_URL, getDisplayedUrl());
    }

    /**
     * Tests that UrlBar TextView is updated when the URL navigated to changes.
     */
    @Test
    @SmallTest
    public void testUrlBarTextViewOnNewNavigation() {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        mActivityTestRule.navigateAndWait(mTestServerSiteUrl);
        Assert.assertEquals(mTestServerSiteUrl, mActivityTestRule.getCurrentDisplayUrl());

        View urlBarView = (LinearLayout) activity.getUrlBarView();

        // Remove everything but the TLD because these aren't displayed.
        String mExpectedUrlBarViewText = mTestServerSiteUrl.substring(HTTP_SCHEME.length());
        mExpectedUrlBarViewText =
                mExpectedUrlBarViewText.substring(0, mExpectedUrlBarViewText.indexOf("/echo"));

        Assert.assertEquals(mExpectedUrlBarViewText, getDisplayedUrl());
    }

    /**
     * Tests that UrlBar TextView is updated when the active tab changes.
     */
    @Test
    @SmallTest
    public void testUrlBarTextViewOnNewActiveTab() throws ExecutionException {
        String url = mActivityTestRule.getTestDataURL(NEW_TAB_URL);
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(url);
        Assert.assertNotNull(activity);

        NewTabCallbackImpl callback = new NewTabCallbackImpl();
        Tab firstTab = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            Tab tab = activity.getBrowser().getActiveTab();
            tab.setNewTabCallback(callback);
            return tab;
        });

        // This should launch a new tab and navigate to about:blank.
        EventUtils.simulateTouchCenterOfView(activity.getWindow().getDecorView());

        callback.waitForNewTab();
        Tab newTab = TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertEquals(2, activity.getBrowser().getTabs().size());
            Tab secondTab = activity.getBrowser().getActiveTab();
            Assert.assertNotSame(firstTab, secondTab);
            return secondTab;
        });

        NavigationWaiter waiter = new NavigationWaiter(ABOUT_BLANK_URL, newTab, false, true);
        if (!ABOUT_BLANK_URL.equals(getDisplayedUrl())) {
            waiter.waitForNavigation();
        }

        Assert.assertEquals(ABOUT_BLANK_URL, getDisplayedUrl());
    }

    @Test
    @SmallTest
    @OverrideTrustedCdn
    public void testTrustedCdn() throws RemoteException {
        Bundle extras = new Bundle();
        extras.putBoolean(InstrumentationActivity.EXTRA_URLBAR_SHOW_PUBLISHER_URL, true);
        InstrumentationActivity activity =
                mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL, extras);
        runTrustedCdnPublisherUrlTest("https://example.com/test", "example.com", getAmpIcon());
    }

    @Test
    @SmallTest
    public void testUntrustedCdn() throws RemoteException {
        Bundle extras = new Bundle();
        extras.putBoolean(InstrumentationActivity.EXTRA_URLBAR_SHOW_PUBLISHER_URL, true);
        InstrumentationActivity activity =
                mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL, extras);
        runTrustedCdnPublisherUrlTest("https://example.com/test", null, getDefaultSecurityIcon());
    }

    @Test
    @SmallTest
    @OverrideTrustedCdn
    @CommandLineFlags.Add("disable-features=ShowTrustedPublisherURL")
    public void testTrustedCdnFeatureDisabled() throws RemoteException {
        Bundle extras = new Bundle();
        extras.putBoolean(InstrumentationActivity.EXTRA_URLBAR_SHOW_PUBLISHER_URL, true);
        InstrumentationActivity activity =
                mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL, extras);
        runTrustedCdnPublisherUrlTest("https://example.com/test", null, getDefaultSecurityIcon());
    }

    @Test
    @SmallTest
    @OverrideTrustedCdn
    public void testTrustedCdnNoHeader() throws RemoteException {
        Bundle extras = new Bundle();
        extras.putBoolean(InstrumentationActivity.EXTRA_URLBAR_SHOW_PUBLISHER_URL, true);
        InstrumentationActivity activity =
                mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL, extras);
        runTrustedCdnPublisherUrlTest(null, null, getDefaultSecurityIcon());
    }

    @Test
    @SmallTest
    @OverrideTrustedCdn
    public void testTrustedCdnNoUrlBarOption() throws RemoteException {
        InstrumentationActivity activity = mActivityTestRule.launchShellWithUrl(ABOUT_BLANK_URL);
        runTrustedCdnPublisherUrlTest("https://example.com/test", null, getDefaultSecurityIcon());
    }
}
