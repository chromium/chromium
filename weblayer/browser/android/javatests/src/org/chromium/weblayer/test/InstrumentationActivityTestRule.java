// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.net.Uri;
import android.os.Bundle;
import android.support.test.InstrumentationRegistry;
import android.support.test.runner.lifecycle.Stage;

import androidx.fragment.app.Fragment;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.weblayer.CookieManager;
import org.chromium.weblayer.NavigationController;
import org.chromium.weblayer.Tab;
import org.chromium.weblayer.WebLayer;
import org.chromium.weblayer.shell.InstrumentationActivity;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * ActivityTestRule for InstrumentationActivity.
 *
 * Test can use this ActivityTestRule to launch or get InstrumentationActivity.
 */
public class InstrumentationActivityTestRule
        extends WebLayerActivityTestRule<InstrumentationActivity> {
    /** The top level key of the JSON object returned by executeScriptSync(). */
    public static final String SCRIPT_RESULT_KEY = "result";

    @Rule
    private EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    private static final class StringCallbackHelper extends CallbackHelper {
        private String mResult;

        public String getResult() {
            return mResult;
        }

        public void notifyCalled(String result) {
            mResult = result;
            notifyCalled();
        }
    }

    public InstrumentationActivityTestRule() {
        super(InstrumentationActivity.class);
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        return super.apply(mTestServerRule.apply(base, description), description);
    }

    public WebLayer getWebLayer() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> { return WebLayer.loadSync(getContextForWebLayer()); });
    }

    public Context getContextForWebLayer() {
        return InstrumentationRegistry.getTargetContext().getApplicationContext();
    }

    /**
     * Starts the WebLayer activity with the given extras Bundle. This does not create and load
     * WebLayer.
     */
    public InstrumentationActivity launchShell(Bundle extras) {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.putExtras(extras);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setComponent(
                new ComponentName(InstrumentationRegistry.getInstrumentation().getTargetContext(),
                        InstrumentationActivity.class));
        launchActivity(intent);
        return getActivity();
    }

    /**
     * Starts the WebLayer activity with the given extras Bundle and completely loads the given URL
     * (this calls navigateAndWait()).
     */
    public InstrumentationActivity launchShellWithUrl(String url, Bundle extras) {
        InstrumentationActivity activity = launchShell(extras);
        Assert.assertNotNull(activity);
        try {
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> activity.loadWebLayerSync(getContextForWebLayer()));
        } catch (ExecutionException e) {
            throw new RuntimeException(e);
        }
        if (url != null) navigateAndWait(url);
        return activity;
    }

    /**
     * Starts the WebLayer activity and completely loads the given URL (this calls
     * navigateAndWait()).
     */
    public InstrumentationActivity launchShellWithUrl(String url) {
        return launchShellWithUrl(url, new Bundle());
    }

    /**
     * Loads the given URL in the shell.
     */
    public void navigateAndWait(String url) {
        navigateAndWait(getActivity().getTab(), url, true /* waitForPaint */);
    }

    public void navigateAndWait(Tab tab, String url, boolean waitForPaint) {
        (new NavigationWaiter(url, tab, false /* expectFailure */, waitForPaint)).navigateAndWait();
    }

    /**
     * Loads the given URL in the shell, expecting failure.
     */
    public void navigateAndWaitForFailure(String url) {
        navigateAndWaitForFailure(getActivity().getTab(), url, true /* waitForPaint */);
    }

    public void navigateAndWaitForFailure(Tab tab, String url, boolean waitForPaint) {
        (new NavigationWaiter(url, tab, true /* expectFailure */, waitForPaint)).navigateAndWait();
    }

    public void recreateByRotatingToLandscape() {
        setActivity(ApplicationTestUtils.waitForActivityWithClass(
                InstrumentationActivity.class, Stage.RESUMED, () -> {
                    getActivity().setRequestedOrientation(
                            ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
                }));
    }

    /**
     * Executes the script passed in and waits for the result. Wraps that result in a JSONObject for
     * convenience of callers that want to process that result as a type other than String.
     */
    public JSONObject executeScriptSync(String script, boolean useSeparateIsolate, Tab tab) {
        StringCallbackHelper callbackHelper = new StringCallbackHelper();
        int count = callbackHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab scriptTab = tab == null ? getActivity().getBrowser().getActiveTab() : tab;
            scriptTab.executeScript(script, useSeparateIsolate,
                    (String result) -> { callbackHelper.notifyCalled(result); });
        });
        try {
            callbackHelper.waitForCallback(count);
        } catch (TimeoutException e) {
            throw new RuntimeException(e);
        }
        JSONObject resultAsJSONObject;
        try {
            resultAsJSONObject = new JSONObject(
                    "{\"" + SCRIPT_RESULT_KEY + "\":" + callbackHelper.getResult() + "}");
        } catch (JSONException e) {
            // This should never happen since the result should be well formed.
            throw new RuntimeException(e);
        }
        return resultAsJSONObject;
    }

    public JSONObject executeScriptSync(String script, boolean useSeparateIsolate) {
        return executeScriptSync(script, useSeparateIsolate, null);
    }

    public int executeScriptAndExtractInt(String script) {
        try {
            return executeScriptSync(script, true /* useSeparateIsolate */)
                    .getInt(SCRIPT_RESULT_KEY);
        } catch (JSONException e) {
            throw new RuntimeException(e);
        }
    }

    public String executeScriptAndExtractString(String script) {
        return executeScriptAndExtractString(script, true /* useSeparateIsolate */);
    }

    public String executeScriptAndExtractString(String script, boolean useSeparateIsolate) {
        try {
            return executeScriptSync(script, useSeparateIsolate).getString(SCRIPT_RESULT_KEY);
        } catch (JSONException e) {
            throw new RuntimeException(e);
        }
    }

    public boolean executeScriptAndExtractBoolean(String script) {
        return executeScriptAndExtractBoolean(script, true /* useSeparateIsolate */);
    }

    public boolean executeScriptAndExtractBoolean(String script, boolean useSeparateIsolate) {
        try {
            return executeScriptSync(script, useSeparateIsolate).getBoolean(SCRIPT_RESULT_KEY);
        } catch (JSONException e) {
            throw new RuntimeException(e);
        }
    }

    public InstrumentationActivity launchWithProfile(String profileName) {
        Bundle extras = new Bundle();
        extras.putString(InstrumentationActivity.EXTRA_PROFILE_NAME, profileName);
        String url = "data:text,foo";
        return launchShellWithUrl(url, extras);
    }

    public EmbeddedTestServer getTestServer() {
        return mTestServerRule.getServer();
    }

    public EmbeddedTestServerRule getTestServerRule() {
        return mTestServerRule;
    }

    public String getTestDataURL(String path) {
        return getTestServer().getURL("/weblayer/test/data/" + path);
    }

    // Returns the URL that is currently being displayed to the user.
    public String getCurrentDisplayUrl() {
        InstrumentationActivity activity = getActivity();
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            NavigationController navigationController =
                    activity.getBrowser().getActiveTab().getNavigationController();

            if (navigationController.getNavigationListSize() == 0) {
                return null;
            }

            // TODO(crbug.com/1066382): This will not be correct in the case where the initial
            // navigation in |tab| was a failed navigation and there have been no more navigations
            // since then.
            return navigationController
                    .getNavigationEntryDisplayUri(
                            navigationController.getNavigationListCurrentIndex())
                    .toString();
        });
    }

    public void setRetainInstance(boolean retain) {
        TestThreadUtils.runOnUiThreadBlocking(() -> getActivity().setRetainInstance(retain));
    }

    public Fragment getFragment() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(() -> getActivity().getFragment());
    }

    public boolean setCookie(CookieManager cookieManager, Uri uri, String value) throws Exception {
        Boolean[] resultHolder = new Boolean[1];
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            cookieManager.setCookie(uri, value, (Boolean result) -> {
                resultHolder[0] = result;
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForFirst();
        return resultHolder[0];
    }

    public String getCookie(CookieManager cookieManager, Uri uri) throws Exception {
        String[] resultHolder = new String[1];
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            cookieManager.getCookie(uri, (String result) -> {
                resultHolder[0] = result;
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForFirst();
        return resultHolder[0];
    }

    public List<String> getResponseCookies(CookieManager cookieManager, Uri uri) throws Exception {
        List<String> finalResult = new ArrayList<>();
        CallbackHelper callbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            cookieManager.getResponseCookies(uri, (List<String> result) -> {
                finalResult.addAll(result);
                callbackHelper.notifyCalled();
            });
        });
        callbackHelper.waitForFirst();
        return finalResult;
    }
}
