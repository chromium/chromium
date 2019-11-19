// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import android.net.Uri;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.weblayer.Navigation;
import org.chromium.weblayer.NavigationCallback;
import org.chromium.weblayer.Tab;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Helper that blocks until a navigation completes.
 */
public class NavigationWaiter {
    private String mUrl;
    private Tab mTab;
    private boolean mNavigationObserved;
    /* True indicates that the expected navigation event is a failure. False indicates that the
     * expected event is completion. */
    private boolean mExpectFailure;
    private boolean mDoneLoading;
    private boolean mContentfulPaint;
    private CallbackHelper mCallbackHelper = new CallbackHelper();

    private NavigationCallback mNavigationCallback = new NavigationCallback() {
        @Override
        public void onNavigationCompleted(Navigation navigation) {
            if (navigation.getUri().toString().equals(mUrl) && !mExpectFailure) {
                mNavigationObserved = true;
                checkComplete();
            }
        }

        @Override
        public void onNavigationFailed(Navigation navigation) {
            if (navigation.getUri().toString().equals(mUrl) && mExpectFailure) {
                mNavigationObserved = true;
                checkComplete();
            }
        }

        @Override
        public void onLoadStateChanged(boolean isLoading, boolean toDifferentDocument) {
            mDoneLoading = !isLoading;
            checkComplete();
        }

        @Override
        public void onFirstContentfulPaint() {
            mContentfulPaint = true;
            checkComplete();
        }
    };

    // |waitForPaint| should generally be set to true, unless there is a specific reason for
    // onFirstContentfulPaint to not fire.
    public NavigationWaiter(
            String url, Tab controller, boolean expectFailure, boolean waitForPaint) {
        mUrl = url;
        mTab = controller;
        if (!waitForPaint) mContentfulPaint = true;
        mExpectFailure = expectFailure;
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTab.getNavigationController().registerNavigationCallback(mNavigationCallback);
        });
    }

    /**
     * Blocks until the navigation specified in the constructor completes.
     *
     * This also cleans up state, so it should only be called once per class instance.
     */
    public void waitForNavigation() {
        try {
            mCallbackHelper.waitForCallback(
                    0, 1, CallbackHelper.WAIT_TIMEOUT_SECONDS * 2, TimeUnit.SECONDS);
        } catch (TimeoutException e) {
            throw new RuntimeException(e);
        }
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTab.getNavigationController().unregisterNavigationCallback(mNavigationCallback);
        });
    }

    public void navigateAndWait() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mTab.getNavigationController().navigate(Uri.parse(mUrl)); });
        waitForNavigation();
    }

    private void checkComplete() {
        if (mNavigationObserved && mDoneLoading && mContentfulPaint) {
            mCallbackHelper.notifyCalled();
        }
    }
}
