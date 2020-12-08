// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import org.hamcrest.Matchers;
import org.junit.Assert;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.FullscreenCallback;

import java.util.concurrent.TimeoutException;

/**
 * FullscreenCallback implementation for tests.
 */
public class TestFullscreenCallback extends FullscreenCallback {
    public int mEnterFullscreenCount;
    public int mExitFullscreenCount;
    public Runnable mExitFullscreenRunnable;
    private int mCallCountToWaitFor;
    private final InstrumentationActivityTestRule mTestRule;
    private final CallbackHelper mCallbackHelper;

    public TestFullscreenCallback(InstrumentationActivityTestRule testRule) {
        mTestRule = testRule;
        mCallbackHelper = new CallbackHelper();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mTestRule.getActivity().getTab().setFullscreenCallback(this); });
    }

    @Override
    public void onEnterFullscreen(Runnable exitFullscreenRunner) {
        mEnterFullscreenCount++;
        mExitFullscreenRunnable = exitFullscreenRunner;
        mCallbackHelper.notifyCalled();
    }

    @Override
    public void onExitFullscreen() {
        mExitFullscreenCount++;
        mCallbackHelper.notifyCalled();
    }

    public void waitForFullscreen() {
        waitForFullscreenImpl(true);
    }

    public void waitForExitFullscreen() {
        waitForFullscreenImpl(false);
    }

    private void waitForFullscreenImpl(boolean isFullscreen) {
        try {
            mCallbackHelper.waitForCallback(mCallCountToWaitFor++);
        } catch (TimeoutException e) {
            Assert.fail("Timeout waiting for fullscreen change");
            return;
        }
        // Handles tests that destroy tab.
        if (mTestRule.getActivity().getTab() == null) return;
        CriteriaHelper.pollInstrumentationThread(
                () -> { Criteria.checkThat(isPageFullscreen(), Matchers.is(isFullscreen)); });
    }

    private boolean isPageFullscreen() {
        return mTestRule.executeScriptAndExtractBoolean("document.fullscreenElement != null");
    }
}
