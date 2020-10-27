// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

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
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.content_public.browser.test.util.ClickUtils;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.ui.test.util.UiRestriction;
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
    // The javascript snippets.
    private static final String UNSET_RESULT_SCRIPT = "lastExecutionResult = null";
    private static final String GET_RESULT_SCRIPT = "lastExecutionResult";
    private static final String WAIT_DEVICE_SCRIPT = "waitUntilDeviceAvailable();";
    private static final String START_PRESENTATION_SCRIPT = "startPresentation();";
    private static final String CHECK_CONNECTION_SCRIPT = "checkConnection();";
    private static final String TERMINATE_CONNECTION_SCRIPT =
            "terminateConnectionAndWaitForStateChange();";

    @Before
    public void setUp() throws Exception {
        mActivity = mActivityTestRule.launchShellWithUrl("about:blank");
        TestWebLayer.getTestWebLayer(mActivity.getApplicationContext())
                .initializeMockMediaRouteProvider();
    }

    private void executeJavaScriptApi(String script) throws Exception {
        mActivityTestRule.executeScriptSync(UNSET_RESULT_SCRIPT, false);
        mActivityTestRule.executeScriptSync(script, false);
        CriteriaHelper.pollInstrumentationThread(() -> {
            String result =
                    mActivityTestRule.executeScriptAndExtractString(GET_RESULT_SCRIPT, false);
            Criteria.checkThat(result, Matchers.is("passed"));
        }, SCRIPT_TIMEOUT_MS, SCRIPT_RETRY_MS);
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
        // Request a presentation.
        mActivityTestRule.navigateAndWait(mActivityTestRule.getTestDataURL(TEST_PAGE));
        executeJavaScriptApi(WAIT_DEVICE_SCRIPT);
        executeJavaScriptApi(START_PRESENTATION_SCRIPT);

        // Verify the route selection dialog is showing and make a selection.
        View testRouteButton = TestWebLayer.getTestWebLayer(mActivity.getApplicationContext())
                                       .getMediaRouteButton(TEST_SINK_NAME);
        Assert.assertNotNull(testRouteButton);
        ClickUtils.mouseSingleClickView(
                InstrumentationRegistry.getInstrumentation(), testRouteButton);

        // Verify in javascript that a presentation has started.
        executeJavaScriptApi(CHECK_CONNECTION_SCRIPT);
        String connectionId =
                mActivityTestRule.executeScriptAndExtractString("startedConnection.id");
        Assert.assertFalse(connectionId.isEmpty());
        String defaultRequestConnectionId =
                mActivityTestRule.executeScriptAndExtractString("defaultRequestConnectionId");
        Assert.assertEquals(connectionId, defaultRequestConnectionId);
        executeJavaScriptApi(TERMINATE_CONNECTION_SCRIPT);
    }
}
