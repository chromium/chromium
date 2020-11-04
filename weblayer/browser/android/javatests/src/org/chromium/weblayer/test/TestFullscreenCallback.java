// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer.test;

import org.hamcrest.Matchers;

import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.weblayer.FullscreenCallback;

/**
 * FullscreenCallback implementation for tests.
 */
public class TestFullscreenCallback extends FullscreenCallback {
    public int mEnterFullscreenCount;
    public int mExitFullscreenCount;
    public Runnable mExitFullscreenRunnable;

    @Override
    public void onEnterFullscreen(Runnable exitFullscreenRunner) {
        mEnterFullscreenCount++;
        mExitFullscreenRunnable = exitFullscreenRunner;
    }

    @Override
    public void onExitFullscreen() {
        mExitFullscreenCount++;
    }

    public void waitForFullscreen() {
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(mEnterFullscreenCount, Matchers.is(1)));
    }

    public void waitForExitFullscreen() {
        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(mExitFullscreenCount, Matchers.is(1)));
    }
}
