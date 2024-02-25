// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.app.Activity;
import android.graphics.Rect;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.View.OnAttachStateChangeListener;
import android.widget.FrameLayout;
import android.widget.FrameLayout.LayoutParams;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadow.api.Shadow;
import org.robolectric.shadows.ShadowView;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.ui.widget.RectProvider.Observer;
import org.chromium.ui.widget.ViewRectProviderTest.MyShadowView;

import java.util.Optional;

/** Unit tests for {@link ViewRectProvider} .*/
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = MyShadowView.class)
public class ViewRectProviderTest {
    private static final int WIDTH = 600;
    private static final int HEIGHT = 800;

    /** Custom ShadowView which includes setter for {@link View#isShown}. **/
    @Implements(View.class)
    public static class MyShadowView extends ShadowView {
        boolean mIsShown;

        /** Empty ctor required for shadow classes. */
        public MyShadowView() {}

        @Implementation
        protected boolean isShown() {
            return mIsShown;
        }

        @Implementation
        protected void getLocationInWindow(int[] outLocation) {
            outLocation[0] = this.realView.getLeft();
            outLocation[1] = this.realView.getTop();
        }

        void setIsShown(boolean isShown) {
            mIsShown = isShown;
        }
    }

    private ViewRectProvider mViewRectProvider;
    private FrameLayout mRootView;
    private View mView;
    private MyShadowView mShadowView;
    private Activity mActivity;
    private RectProvider.Observer mObserver;

    private CallbackHelper mOnRectChangeCallback;
    private CallbackHelper mOnRectHideCallback;

    @Before
    public void setup() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        mRootView = new FrameLayout(mActivity);
        mActivity.setContentView(mRootView);

        mView = new View(mActivity);

        mRootView.addView(
                mView, new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        mViewRectProvider = new ViewRectProvider(mView);

        mOnRectChangeCallback = new CallbackHelper();
        mOnRectHideCallback = new CallbackHelper();
        mObserver =
                new Observer() {
                    @Override
                    public void onRectChanged() {
                        mOnRectChangeCallback.notifyCalled();
                    }

                    @Override
                    public void onRectHidden() {
                        mOnRectHideCallback.notifyCalled();
                    }
                };

        View rootView = mRootView.getRootView();
        rootView.measure(
                MeasureSpec.makeMeasureSpec(WIDTH, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(HEIGHT, MeasureSpec.EXACTLY));
        rootView.layout(0, 0, WIDTH, HEIGHT);

        mShadowView = Shadow.extract(mView);
        mShadowView.setIsShown(true);

        mViewRectProvider.startObserving(mObserver);
    }

    @Test
    public void testProviderViewBound() {
        assertRectMatch(0, 0, WIDTH, HEIGHT);
    }

    @Test
    public void testOnRectHidden() {
        mShadowView.setIsShown(false);

        int expectedCounts = 0;
        mView.getViewTreeObserver().dispatchOnGlobalLayout();
        Assert.assertEquals(
                "#onGlobalLayout should call #onRectHidden.",
                ++expectedCounts,
                mOnRectHideCallback.getCallCount());

        mView.getViewTreeObserver().dispatchOnPreDraw();
        Assert.assertEquals(
                "#onPreDraw should call #onRectHidden.",
                ++expectedCounts,
                mOnRectHideCallback.getCallCount());

        Optional<OnAttachStateChangeListener> listener =
                mShadowView.getOnAttachStateChangeListeners().stream().findFirst();
        Assert.assertTrue(listener.isPresent());
        listener.get().onViewDetachedFromWindow(mView);
        Assert.assertEquals(
                "#onViewDetachedFromWindow should call #onRectHidden.",
                ++expectedCounts,
                mOnRectHideCallback.getCallCount());
    }

    @Test
    public void testOnRectChanged() {
        int expectedCounts = 0;

        mView.layout(0, 0, WIDTH, HEIGHT);
        mView.getViewTreeObserver().dispatchOnPreDraw();
        Assert.assertEquals(
                "View does not changes its bound. Should not trigger #onRectChanged.",
                expectedCounts,
                mOnRectChangeCallback.getCallCount());
        mView.layout(0, 0, 100, 100);
        mView.getViewTreeObserver().dispatchOnPreDraw();
        Assert.assertEquals(
                "View changing its bound should trigger #onRectChanged.",
                ++expectedCounts,
                mOnRectChangeCallback.getCallCount());
        assertRectMatch(0, 0, 100, 100);

        mView.layout(1, 1, 101, 101);
        mView.getViewTreeObserver().dispatchOnPreDraw();
        Assert.assertEquals(
                "View changing its position on screen should trigger #onRectChanged.",
                ++expectedCounts,
                mOnRectChangeCallback.getCallCount());
        assertRectMatch(1, 1, 101, 101);

        mViewRectProvider.setInsetPx(new Rect(0, 0, 0, 0));
        Assert.assertEquals(
                "Setting same view inset should not trigger #onRectChanged.",
                expectedCounts,
                mOnRectChangeCallback.getCallCount());
        mViewRectProvider.setInsetPx(new Rect(0, 0, 0, 1));
        Assert.assertEquals(
                "Setting different inset on ViewRectProvider should trigger #onRectChanged.",
                ++expectedCounts,
                mOnRectChangeCallback.getCallCount());
        assertRectMatch(1, 1, 101, 100);

        mViewRectProvider.setIncludePadding(false);
        Assert.assertEquals(
                "Setting same ViewProvider#setIncludePadding should not trigger #onRectChanged.",
                expectedCounts,
                mOnRectChangeCallback.getCallCount());
        mViewRectProvider.setIncludePadding(true);
        Assert.assertEquals(
                "Setting different ViewProvider#setIncludePadding should trigger #onRectChanged.",
                ++expectedCounts,
                mOnRectChangeCallback.getCallCount());
    }

    private void assertRectMatch(int left, int top, int right, int bottom) {
        final Rect expectedRect = new Rect(left, top, right, bottom);
        Assert.assertEquals("Rect does not match.", expectedRect, mViewRectProvider.getRect());
    }
}
