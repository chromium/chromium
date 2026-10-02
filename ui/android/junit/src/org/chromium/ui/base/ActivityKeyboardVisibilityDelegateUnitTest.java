// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.graphics.Insets;
import android.view.View.MeasureSpec;
import android.view.WindowInsets;
import android.widget.FrameLayout;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
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

    private class TestRootView extends FrameLayout {
        TestRootView(Activity activity) {
            super(activity);
        }

        // Robolectric does not provide a way to set the root window insets.
        @Override
        public WindowInsets getRootWindowInsets() {
            return mWindowInsets;
        }
    }

    @Mock private KeyboardVisibilityListener mKeyboardVisibilityListener;
    @Mock private WindowInsets mWindowInsets;

    private TestRootView mRootView;
    private final SettableMonotonicObservableSupplier<Integer> mKeyboardInsetSupplier =
            ObservableSuppliers.createMonotonic();
    private LazyOneshotSupplier<MonotonicObservableSupplier<Integer>> mLazyKeyboardInsetSupplier;
    private ActivityKeyboardVisibilityDelegate mKeyboardVisibilityDelegate;

    @Before
    public void setUp() {
        mLazyKeyboardInsetSupplier = LazyOneshotSupplier.fromValue(mKeyboardInsetSupplier);
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(Activity activity) {
        // Not attached to the activity's window so that it is its own root view.
        mRootView = new TestRootView(activity);
        mKeyboardVisibilityDelegate =
                new ActivityKeyboardVisibilityDelegate(new WeakReference<>(activity));
        mKeyboardVisibilityDelegate.setContentViewForTesting(mRootView);
        setRootViewKeyboardInset(0);
    }

    @Test
    public void testOnLayoutChangeObserver() {
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityListener);
        assertEquals(1, shadowOf(mRootView).getOnLayoutChangeListeners().size());

        int inset = 150;
        setRootViewKeyboardInset(inset);
        triggerLayout();
        // Verify the observer is notified the keyboard is shown in response to a layout change.
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(true);

        inset = 0;
        setRootViewKeyboardInset(inset);
        triggerLayout();
        // Verify the observer is notified the keyboard is hidden in response to a layout change.
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(false);
    }

    @Test
    public void testKeyboardInsetObserver_ObserverAlreadyAdded() {
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityListener);
        assertEquals(1, shadowOf(mRootView).getOnLayoutChangeListeners().size());
        mKeyboardVisibilityDelegate.setLazyKeyboardInsetSupplier(mLazyKeyboardInsetSupplier);
        assertTrue(mKeyboardInsetSupplier.hasObservers());

        int inset = 150;
        setRootViewKeyboardInset(inset);
        mKeyboardInsetSupplier.set(inset);
        // Verify the observer is notified the keyboard is shown in response to an inset change.
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(true);

        // Verify only called once.
        triggerLayout();
        // Verify the observer is not re-notified the keyboard is shown.
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(true);

        inset = 0;
        setRootViewKeyboardInset(inset);
        mKeyboardInsetSupplier.set(inset);
        // Verify the observer is notified the keyboard is hidden in response to an inset change.
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(false);

        // Verify only called once.
        triggerLayout();
        // Verify the observer is not re-notified the keyboard is hidden.
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(false);
    }

    @Test
    public void testKeyboardInsetObserver_ObserverNotYetAdded() {
        mKeyboardVisibilityDelegate.setLazyKeyboardInsetSupplier(mLazyKeyboardInsetSupplier);
        assertFalse(mKeyboardInsetSupplier.hasObservers());

        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityListener);
        assertEquals(1, shadowOf(mRootView).getOnLayoutChangeListeners().size());
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

    private void triggerLayout() {
        mRootView.forceLayout();
        int spec = MeasureSpec.makeMeasureSpec(100, MeasureSpec.EXACTLY);
        mRootView.measure(spec, spec);
        mRootView.layout(0, 0, 100, 100);
    }

    private void setRootViewKeyboardInset(int inset) {
        when(mWindowInsets.getInsets(WindowInsets.Type.systemBars()))
                .thenReturn(Insets.of(0, 0, 0, 0));
        when(mWindowInsets.getInsetsIgnoringVisibility(WindowInsets.Type.navigationBars()))
                .thenReturn(Insets.of(0, 0, 0, 0));
        when(mWindowInsets.getInsets(WindowInsets.Type.ime()))
                .thenReturn(Insets.of(0, 0, 0, inset));
    }
}
