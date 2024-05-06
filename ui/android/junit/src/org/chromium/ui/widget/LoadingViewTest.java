// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.app.Activity;
import android.view.View;
import android.widget.FrameLayout;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowView;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;

import java.util.concurrent.TimeUnit;

/** Tests for {@link LoadingView}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowView.class})
@LooperMode(LooperMode.Mode.LEGACY)
public class LoadingViewTest {
    static class TestObserver implements LoadingView.Observer {
        public final CallbackHelper showLoadingCallback = new CallbackHelper();
        public final CallbackHelper hideLoadingCallback = new CallbackHelper();

        @Override
        public void onShowLoadingUIComplete() {
            showLoadingCallback.notifyCalled();
        }

        @Override
        public void onHideLoadingUIComplete() {
            hideLoadingCallback.notifyCalled();
        }
    }

    private LoadingView mLoadingView;
    private Activity mActivity;
    private final TestObserver mTestObserver1 = new TestObserver();
    private final TestObserver mTestObserver2 = new TestObserver();

    @Before
    public void setUpTest() throws Exception {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();

        FrameLayout content = new FrameLayout(mActivity);
        mActivity.setContentView(content);

        mLoadingView = new LoadingView(mActivity);
        LoadingView.setDisableAnimationForTest(true);
        content.addView(mLoadingView);

        mLoadingView.addObserver(mTestObserver1);
        mLoadingView.addObserver(mTestObserver2);
    }

    @Test
    @SmallTest
    public void testLoadingFast() {
        mLoadingView.showLoadingUI();
        Assert.assertEquals(
                "showLoadingCallback1 should not be executed as soon as showLoadingUI is called.",
                0,
                mTestObserver1.showLoadingCallback.getCallCount());
        Assert.assertEquals(
                "showLoadingCallback2 should not be executed as soon as showLoadingUI is called.",
                0,
                mTestObserver2.showLoadingCallback.getCallCount());

        ShadowLooper.idleMainLooper(100, TimeUnit.MILLISECONDS);
        Assert.assertEquals(
                "Progress bar should be hidden before 500ms.",
                View.GONE,
                mLoadingView.getVisibility());
        Assert.assertEquals(
                "showLoadingCallback1 should not be executed with loading fast.",
                0,
                mTestObserver1.showLoadingCallback.getCallCount());
        Assert.assertEquals(
                "showLoadingCallback2 should not be executed with loading fast.",
                0,
                mTestObserver2.showLoadingCallback.getCallCount());

        mLoadingView.hideLoadingUI();
        Assert.assertEquals(
                "Progress bar should never be visible.", View.GONE, mLoadingView.getVisibility());
        Assert.assertEquals(
                "hideLoadingCallback1 should be executed after loading finishes.",
                1,
                mTestObserver1.hideLoadingCallback.getCallCount());
        Assert.assertEquals(
                "hideLoadingCallback2 should be executed after loading finishes.",
                1,
                mTestObserver2.hideLoadingCallback.getCallCount());
    }

    @Test
    @SmallTest
    public void testLoadingSlow() {
        long sleepTime = 500;
        mLoadingView.showLoadingUI();
        Assert.assertEquals(
                "showLoadingCallback1 should not be executed as soon as showLoadingUI is called.",
                0,
                mTestObserver1.showLoadingCallback.getCallCount());
        Assert.assertEquals(
                "showLoadingCallback2 should not be executed as soon as showLoadingUI is called.",
                0,
                mTestObserver2.showLoadingCallback.getCallCount());

        ShadowLooper.idleMainLooper(sleepTime, TimeUnit.MILLISECONDS);
        Assert.assertEquals(
                "Progress bar should be visible after 500ms.",
                View.VISIBLE,
                mLoadingView.getVisibility());
        Assert.assertEquals(
                "showLoadingCallback1 should be executed when spinner is visible.",
                1,
                mTestObserver1.showLoadingCallback.getCallCount());
        Assert.assertEquals(
                "showLoadingCallback2 should be executed when spinner is visible.",
                1,
                mTestObserver2.showLoadingCallback.getCallCount());

        mLoadingView.hideLoadingUI();
        Assert.assertEquals(
                "Progress bar should still be visible until showing for 500ms.",
                View.VISIBLE,
                mLoadingView.getVisibility());
        Assert.assertEquals(
                "hideLoadingCallback1 should not be executed before loading finishes.",
                0,
                mTestObserver1.hideLoadingCallback.getCallCount());
        Assert.assertEquals(
                "hideLoadingCallback2 should not be executed before loading finishes.",
                0,
                mTestObserver2.hideLoadingCallback.getCallCount());

        // The spinner should be displayed for at least 500ms.
        ShadowLooper.idleMainLooper(sleepTime, TimeUnit.MILLISECONDS);
        Assert.assertEquals(
                "Progress bar should be hidden after 500ms.",
                View.GONE,
                mLoadingView.getVisibility());
        Assert.assertEquals(
                "hideLoadingCallback1 should be executed after loading finishes.",
                1,
                mTestObserver1.hideLoadingCallback.getCallCount());
        Assert.assertEquals(
                "hideLoadingCallback2 should be executed after loading finishes.",
                1,
                mTestObserver2.hideLoadingCallback.getCallCount());
    }

    @Test
    @SmallTest
    public void testLoadingSkipDelay() {
        mLoadingView.showLoadingUI(/* skipDelay= */ true);
        Assert.assertEquals(
                "showLoadingCallback1 should be executed as soon as showLoadingUI is called.",
                1,
                mTestObserver1.showLoadingCallback.getCallCount());
        Assert.assertEquals(
                "showLoadingCallback2 should be executed as soon as showLoadingUI is called.",
                1,
                mTestObserver2.showLoadingCallback.getCallCount());
    }
}
