// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.support.test.InstrumentationRegistry;
import android.view.View;

import androidx.test.filters.LargeTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.content_public.browser.test.util.ClickUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.ui.test.util.UiRestriction;
import org.chromium.weblayer.Browser;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.TestWebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

/**
 * Tests of the Presentation API.
 */
@MinWebLayerVersion(88)
@RunWith(WebLayerJUnit4ClassRunner.class)
@CommandLineFlags.Add({ContentSwitches.DISABLE_GESTURE_REQUIREMENT_FOR_PRESENTATION})
public class MediaRouterTest {
    @Rule
    public InstrumentationActivityTestRule mActivityTestRule =
            new InstrumentationActivityTestRule();

    private InstrumentationActivity mActivity;

    private static final String TEST_PAGE = "media_router/basic_test.html";

    private static final int SCRIPT_TIMEOUT_MS = 10000;
    private static final int SCRIPT_RETRY_MS = 150;

    private static final String TEST_SINK_NAME = "test-sink-1";

    // Javascript snippets.
    private static final String WAIT_DEVICE_SCRIPT = "waitUntilDeviceAvailable();";
    private static final String START_PRESENTATION_SCRIPT = "startPresentation();";
    private static final String TERMINATE_CONNECTION_SCRIPT =
            "terminateConnectionAndWaitForStateChange();";

    @Before
    public void setUp() {
        mActivity = mActivityTestRule.launchShellWithUrl("about:blank");
    }

    private TestWebLayer getTestWebLayer() {
        return TestWebLayer.getTestWebLayer(mActivity.getApplicationContext());
    }

    private void executeScriptAndWaitForResult(String script) throws Exception {
        mActivityTestRule.executeScriptSync("lastExecutionResult = null", false);
        mActivityTestRule.executeScriptSync(script, false);
        CriteriaHelper.pollInstrumentationThread(() -> {
            String result =
                    mActivityTestRule.executeScriptAndExtractString("lastExecutionResult", false);
            Criteria.checkThat(result, Matchers.is("passed"));
        }, SCRIPT_TIMEOUT_MS, SCRIPT_RETRY_MS);
    }

    private void startPresentationAndSelectRoute() throws Exception {
        // Request a presentation.
        mActivityTestRule.navigateAndWait(mActivityTestRule.getTestDataURL(TEST_PAGE));
        executeScriptAndWaitForResult(WAIT_DEVICE_SCRIPT);
        executeScriptAndWaitForResult(START_PRESENTATION_SCRIPT);

        // Verify the route selection dialog is showing and make a selection.
        View testRouteButton = getTestWebLayer().getMediaRouteButton(TEST_SINK_NAME);
        Assert.assertNotNull(testRouteButton);
        ClickUtils.mouseSingleClickView(
                InstrumentationRegistry.getInstrumentation(), testRouteButton);
    }

    private String verifyPresentationStarted() throws Exception {
        // Verify in javascript that a presentation has started.
        executeScriptAndWaitForResult("checkConnection();");
        String connectionId =
                mActivityTestRule.executeScriptAndExtractString("startedConnection.id", false);
        Assert.assertFalse(connectionId.isEmpty());
        String defaultRequestConnectionId = mActivityTestRule.executeScriptAndExtractString(
                "defaultRequestConnectionId", false);
        Assert.assertEquals(connectionId, defaultRequestConnectionId);
        return connectionId;
    }

    void checkStartFailed(String errorName, String errorMessageSubstring) throws Exception {
        String script =
                String.format("checkStartFailed('%s', '%s');", errorName, errorMessageSubstring);
        executeScriptAndWaitForResult(script);
    }

    /**
     * Basic test where the page requests a route, the user selects a route, and a connection is
     * started.
     */
    @Test
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"MediaRouter"})
    @LargeTest
    public void testBasic() throws Exception {
        getTestWebLayer().initializeMockMediaRouteProvider(/*closeRouteWithErrorOnSend=*/false,
                /*disableIsSupportsSource=*/false, /*createRouteErrorMessage=*/null,
                /*joinRouteErrorMessage=*/null);
        startPresentationAndSelectRoute();
        verifyPresentationStarted();

        executeScriptAndWaitForResult(TERMINATE_CONNECTION_SCRIPT);
    }

    /** Test of PresentationConnection.onmessage. */
    @Test
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"MediaRouter"})
    @LargeTest
    public void testSendAndOnMessage() throws Exception {
        getTestWebLayer().initializeMockMediaRouteProvider(/*closeRouteWithErrorOnSend=*/false,
                /*disableIsSupportsSource=*/false, /*createRouteErrorMessage=*/null,
                /*joinRouteErrorMessage=*/null);
        startPresentationAndSelectRoute();
        verifyPresentationStarted();

        executeScriptAndWaitForResult("sendMessageAndExpectResponse('foo');");
    }

    /** Test of PresentationConnection.onclose. */
    @Test
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"MediaRouter"})
    @LargeTest
    public void testOnClose() throws Exception {
        getTestWebLayer().initializeMockMediaRouteProvider(/*closeRouteWithErrorOnSend=*/true,
                /*disableIsSupportsSource=*/false, /*createRouteErrorMessage=*/null,
                /*joinRouteErrorMessage=*/null);
        startPresentationAndSelectRoute();
        verifyPresentationStarted();

        executeScriptAndWaitForResult("sendMessageAndExpectConnectionCloseOnError()");
    }

    /**
     * Test that starting the presentation fails when there are no providers that support the given
     * source.
     */
    @Test
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"MediaRouter"})
    @LargeTest
    public void testFailNoProvider() throws Exception {
        getTestWebLayer().initializeMockMediaRouteProvider(/*closeRouteWithErrorOnSend=*/false,
                /*disableIsSupportsSource=*/true, /*createRouteErrorMessage=*/null,
                /*joinRouteErrorMessage=*/null);

        startPresentationAndSelectRoute();
        checkStartFailed("UnknownError", "No provider supports createRoute with source");
    }

    /** Tests route creation failure. */
    @Test
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"MediaRouter"})
    @LargeTest
    @FlakyTest(message = "https://crbug.com/1181337")
    public void testFailCreateRoute() throws Exception {
        getTestWebLayer().initializeMockMediaRouteProvider(/*closeRouteWithErrorOnSend=*/false,
                /*disableIsSupportsSource=*/false, /*createRouteErrorMessage=*/"Unknown sink",
                /*joinRouteErrorMessage=*/null);

        startPresentationAndSelectRoute();
        checkStartFailed("UnknownError", "Unknown sink");
    }

    /** Tests reconnecting to a presentation (joining a route) from a new tab. */
    @Test
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"MediaRouter"})
    @LargeTest
    public void testJoinRoute() throws Exception {
        getTestWebLayer().initializeMockMediaRouteProvider(/*closeRouteWithErrorOnSend=*/false,
                /*disableIsSupportsSource=*/false, /*createRouteErrorMessage=*/null,
                /*joinRouteErrorMessage=*/null);

        startPresentationAndSelectRoute();
        String connectionId = verifyPresentationStarted();

        Tab firstTab = mActivity.getTab();
        Tab secondTab = runOnUiThreadBlocking(() -> {
            Browser browser = mActivity.getTab().getBrowser();
            Tab tab = browser.createTab();
            browser.setActiveTab(tab);
            return tab;
        });
        mActivityTestRule.navigateAndWait(
                secondTab, mActivityTestRule.getTestDataURL(TEST_PAGE), true);
        executeScriptAndWaitForResult(String.format("reconnectConnection(\'%s\');", connectionId));
        String reconnectedConnectionId =
                mActivityTestRule.executeScriptAndExtractString("reconnectedConnection.id", false);
        Assert.assertEquals(connectionId, reconnectedConnectionId);

        runOnUiThreadBlocking(() -> { firstTab.getBrowser().setActiveTab(firstTab); });
        executeScriptAndWaitForResult(TERMINATE_CONNECTION_SCRIPT);
    }

    /** Tests failure of reconnecting to a presentation (joining a route) from a new tab. */
    @Test
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"MediaRouter"})
    @LargeTest
    public void testFailureToJoinRoute() throws Exception {
        getTestWebLayer().initializeMockMediaRouteProvider(/*closeRouteWithErrorOnSend=*/false,
                /*disableIsSupportsSource=*/false, /*createRouteErrorMessage=*/null,
                /*joinRouteErrorMessage=*/"Unknown route");

        startPresentationAndSelectRoute();
        String connectionId = verifyPresentationStarted();

        Tab secondTab = runOnUiThreadBlocking(() -> {
            Browser browser = mActivity.getTab().getBrowser();
            Tab tab = browser.createTab();
            browser.setActiveTab(tab);
            return tab;
        });
        mActivityTestRule.navigateAndWait(
                secondTab, mActivityTestRule.getTestDataURL(TEST_PAGE), true);
        executeScriptAndWaitForResult(
                String.format("reconnectConnectionAndExpectFailure(\'%s\');", connectionId));
    }

    /** Tests the user cancelling the media route selection process. */
    @Test
    @Restriction({UiRestriction.RESTRICTION_TYPE_PHONE, RESTRICTION_TYPE_NON_LOW_END_DEVICE})
    @Feature({"MediaRouter"})
    @DisabledTest(message = "https://crbug.com/1144233")
    @LargeTest
    public void testFailStartCancelled() throws Exception {
        getTestWebLayer().initializeMockMediaRouteProvider(/*closeRouteWithErrorOnSend=*/false,
                /*disableIsSupportsSource=*/false, /*createRouteErrorMessage=*/null,
                /*joinRouteErrorMessage=*/null);

        // Request a presentation.
        mActivityTestRule.navigateAndWait(mActivityTestRule.getTestDataURL(TEST_PAGE));
        executeScriptAndWaitForResult(WAIT_DEVICE_SCRIPT);
        executeScriptAndWaitForResult(START_PRESENTATION_SCRIPT);

        // Verify the route selection dialog is showing but then dismiss it.
        View testRouteButton = getTestWebLayer().getMediaRouteButton(TEST_SINK_NAME);
        Assert.assertNotNull(testRouteButton);

        // Click outside the dialog to dismiss it.
        View topContents = mActivity.getTopContentsContainer();
        TestTouchUtils.singleClick(
                InstrumentationRegistry.getInstrumentation(), 1, topContents.getHeight() + 10);
        checkStartFailed("NotAllowedError", "Dialog closed.");
    }
}
