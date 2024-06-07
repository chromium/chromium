// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Insets;
import android.view.View;
import android.view.WindowInsets;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.KeyboardVisibilityDelegate.KeyboardVisibilityListener;

import java.lang.ref.WeakReference;

/** Unit tests for {@link ActivityKeyboardVisibilityDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 30)
public class ActivityKeyboardVisibilityDelegateUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private KeyboardVisibilityListener mKeyboardVisibilityListener;
    @Mock private WindowInsets mWindowInsets;
    @Captor private ArgumentCaptor<View.OnLayoutChangeListener> mOnLayoutChangeListener;

    private FrameLayout mRootView;
    private ObservableSupplierImpl<Integer> mKeyboardInsetSupplier = new ObservableSupplierImpl<>();
    private LazyOneshotSupplier<ObservableSupplier<Integer>> mLazyKeyboardInsetSupplier;
    private ActivityKeyboardVisibilityDelegate mKeyboardVisibilityDelegate;

    @Before
    public void setUp() {
        mLazyKeyboardInsetSupplier = LazyOneshotSupplier.fromValue(mKeyboardInsetSupplier);
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        mRootView = spy(new FrameLayout(activity));
        mRootView.setId(android.R.id.content);
        when(mRootView.getRootView()).thenReturn(mRootView);
        when(mRootView.getRootWindowInsets()).thenReturn(mWindowInsets);
        when(mRootView.isAttachedToWindow()).thenReturn(false);
        activity.setContentView(mRootView);
        mKeyboardVisibilityDelegate =
                new ActivityKeyboardVisibilityDelegate(new WeakReference<Activity>(activity));
        mKeyboardVisibilityDelegate.setContentViewForTesting(mRootView);
        setRootViewKeyboardInset(0);
    }

    @Test
    public void testOnLayoutChangeObserver() {
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityListener);
        verify(mRootView).addOnLayoutChangeListener(mOnLayoutChangeListener.capture());

        int inset = 150;
        setRootViewKeyboardInset(inset);
        mOnLayoutChangeListener.getValue().onLayoutChange(mRootView, 0, 0, 0, 0, 0, 0, 0, 0);
        // Verify the observer is notified the keyboard is shown in response to a layout change.
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(true);

        inset = 0;
        setRootViewKeyboardInset(inset);
        mOnLayoutChangeListener.getValue().onLayoutChange(mRootView, 0, 0, 0, 0, 0, 0, 0, 0);
        // Verify the observer is notified the keyboard is hidden in response to a layout change.
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(false);
    }

    @Test
    public void testKeyboardInsetObserver_ObserverAlreadyAdded() {
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityListener);
        verify(mRootView).addOnLayoutChangeListener(mOnLayoutChangeListener.capture());
        mKeyboardVisibilityDelegate.setLazyKeyboardInsetSupplier(mLazyKeyboardInsetSupplier);
        assertTrue(mKeyboardInsetSupplier.hasObservers());

        int inset = 150;
        setRootViewKeyboardInset(inset);
        mKeyboardInsetSupplier.set(inset);
        // Verify the observer is notified the keyboard is shown in response to an inset change.
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(true);

        // Verify only called once.
        mOnLayoutChangeListener.getValue().onLayoutChange(mRootView, 0, 0, 0, 0, 0, 0, 0, 0);
        // Verify the observer is not re-notified the keyboard is shown.
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(true);

        inset = 0;
        setRootViewKeyboardInset(inset);
        mKeyboardInsetSupplier.set(inset);
        // Verify the observer is notified the keyboard is hidden in response to an inset change.
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(false);

        // Verify only called once.
        mOnLayoutChangeListener.getValue().onLayoutChange(mRootView, 0, 0, 0, 0, 0, 0, 0, 0);
        // Verify the observer is not re-notified the keyboard is hidden.
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(false);
    }

    @Test
    public void testKeyboardInsetObserver_ObserverNotYetAdded() {
        mKeyboardVisibilityDelegate.setLazyKeyboardInsetSupplier(mLazyKeyboardInsetSupplier);
        assertFalse(mKeyboardInsetSupplier.hasObservers());

        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityListener);
        verify(mRootView).addOnLayoutChangeListener(mOnLayoutChangeListener.capture());
        assertTrue(mKeyboardInsetSupplier.hasObservers());

        int inset = 150;
        setRootViewKeyboardInset(inset);
        mKeyboardInsetSupplier.set(inset);
        // Verify the observer is notified the keyboard is shown in response to an inset change.
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(true);

        inset = 0;
        setRootViewKeyboardInset(inset);
        mKeyboardInsetSupplier.set(inset);
        // Verify the observer is notified the keyboard is hidden in response to an inset change.
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(false);
    }

    private void setRootViewKeyboardInset(int inset) {
        when(mWindowInsets.getInsets(WindowInsets.Type.systemBars()))
                .thenReturn(Insets.of(0, 0, 0, 0));
        when(mWindowInsets.getInsets(WindowInsets.Type.ime()))
                .thenReturn(Insets.of(0, 0, 0, inset));
    }
}
