// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;
import android.graphics.Insets;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.WindowInsets;
import android.widget.FrameLayout;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Answers;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.BaseFeatures;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
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

    @Mock(answer = Answers.CALLS_REAL_METHODS)
    private KeyboardVisibilityListener mKeyboardVisibilityListener;

    @Mock private WindowInsets mWindowInsets;

    private TestRootView mRootView;
    private final SettableMonotonicObservableSupplier<Integer> mKeyboardInsetSupplier =
            ObservableSuppliers.createMonotonic();
    private final SettableNonNullObservableSupplier<Boolean> mImeVisibilitySupplier =
            ObservableSuppliers.createNonNull(false);
    private ActivityKeyboardVisibilityDelegate mKeyboardVisibilityDelegate;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    @Test
    public void testSetKeyboardInsetSupplier_observerAlreadyAdded_replacesObserver() {
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityListener);
        mKeyboardVisibilityDelegate.setKeyboardInsetSupplier(mKeyboardInsetSupplier);
        assertTrue(mKeyboardInsetSupplier.hasObservers());

        SettableMonotonicObservableSupplier<Integer> newSupplier =
                ObservableSuppliers.createMonotonic();
        mKeyboardVisibilityDelegate.setKeyboardInsetSupplier(newSupplier);
        assertFalse(mKeyboardInsetSupplier.hasObservers());
        assertTrue(newSupplier.hasObservers());
    }

    private void onActivity(Activity activity) {
        // Not attached to the activity's window so that it is its own root view.
        mRootView = new TestRootView(activity);
        mKeyboardVisibilityDelegate =
                new ActivityKeyboardVisibilityDelegate(new WeakReference<>(activity));
        mKeyboardVisibilityDelegate.setContentViewForTesting(mRootView);
        mKeyboardVisibilityDelegate.setImeVisibilitySupplier(mImeVisibilitySupplier);
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
        verify(mKeyboardVisibilityListener).keyboardObscuringChanged(true);
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(true);

        inset = 0;
        setRootViewKeyboardInset(inset);
        triggerLayout();
        // Verify the observer is notified the keyboard is hidden in response to a layout change.
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(false);
        verify(mKeyboardVisibilityListener).keyboardObscuringChanged(false);
    }

    @Test
    public void testKeyboardInsetObserver_ObserverAlreadyAdded() {
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityListener);
        assertEquals(1, shadowOf(mRootView).getOnLayoutChangeListeners().size());
        mKeyboardVisibilityDelegate.setKeyboardInsetSupplier(mKeyboardInsetSupplier);
        assertTrue(mKeyboardInsetSupplier.hasObservers());

        int inset = 150;
        setRootViewKeyboardInset(inset);
        mKeyboardInsetSupplier.set(inset);
        // Verify the observer is notified the keyboard is shown in response to an inset change.
        verify(mKeyboardVisibilityListener).keyboardObscuringChanged(true);
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(true);

        // Verify only called once.
        triggerLayout();
        // Verify the observer is not re-notified the keyboard is shown.
        verify(mKeyboardVisibilityListener, times(1)).keyboardObscuringChanged(true);
        verify(mKeyboardVisibilityListener, times(1)).keyboardVisibilityChanged(true);

        inset = 0;
        setRootViewKeyboardInset(inset);
        mKeyboardInsetSupplier.set(inset);
        // Verify the observer is notified the keyboard is hidden in response to an inset change.
        verify(mKeyboardVisibilityListener).keyboardObscuringChanged(false);
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(false);

        // Verify only called once.
        triggerLayout();
        // Verify the observer is not re-notified the keyboard is hidden.
        verify(mKeyboardVisibilityListener, times(1)).keyboardObscuringChanged(false);
        verify(mKeyboardVisibilityListener, times(1)).keyboardVisibilityChanged(false);

        mKeyboardVisibilityDelegate.removeKeyboardVisibilityListener(mKeyboardVisibilityListener);
        assertFalse(mKeyboardInsetSupplier.hasObservers());
    }

    @Test
    public void testKeyboardInsetObserver_ObserverNotYetAdded() {
        mKeyboardVisibilityDelegate.setKeyboardInsetSupplier(mKeyboardInsetSupplier);
        assertFalse(mKeyboardInsetSupplier.hasObservers());

        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityListener);
        assertEquals(1, shadowOf(mRootView).getOnLayoutChangeListeners().size());
        assertTrue(mKeyboardInsetSupplier.hasObservers());

        int inset = 150;
        setRootViewKeyboardInset(inset);
        mKeyboardInsetSupplier.set(inset);
        // Verify the observer is notified the keyboard is shown in response to an inset change.
        verify(mKeyboardVisibilityListener).keyboardObscuringChanged(true);
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(true);

        inset = 0;
        setRootViewKeyboardInset(inset);
        mKeyboardInsetSupplier.set(inset);
        // Verify the observer is notified the keyboard is hidden in response to an inset change.
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(false);
        verify(mKeyboardVisibilityListener).keyboardObscuringChanged(false);

        mKeyboardVisibilityDelegate.removeKeyboardVisibilityListener(mKeyboardVisibilityListener);
        assertFalse(mKeyboardInsetSupplier.hasObservers());
    }

    @Test
    @EnableFeatures(BaseFeatures.SPLIT_SCREEN_IME_INSET_VISIBILITY)
    public void testKeyboardInsetObserver_SplitScreenZeroKeyboardInset() {
        mKeyboardVisibilityDelegate.setKeyboardInsetSupplier(mKeyboardInsetSupplier);
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityListener);

        when(mWindowInsets.isVisible(WindowInsets.Type.ime())).thenReturn(true);
        triggerLayout();
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(true);
        verify(mKeyboardVisibilityListener, never()).keyboardObscuringChanged(true);

        mKeyboardInsetSupplier.set(0);
        verify(mKeyboardVisibilityListener, never()).keyboardVisibilityChanged(false);
        verify(mKeyboardVisibilityListener, never()).keyboardObscuringChanged(false);
    }

    @Test
    @DisableFeatures(BaseFeatures.SPLIT_SCREEN_IME_INSET_VISIBILITY)
    public void testKeyboardInsetObserver_SplitScreenZeroKeyboardInset_KillSwitchDisabled() {
        mKeyboardVisibilityDelegate.setKeyboardInsetSupplier(mKeyboardInsetSupplier);
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityListener);

        setRootViewKeyboardInset(150);
        triggerLayout();
        verify(mKeyboardVisibilityListener).keyboardObscuringChanged(true);

        when(mWindowInsets.getInsets(WindowInsets.Type.ime())).thenReturn(Insets.of(0, 0, 0, 0));
        when(mWindowInsets.isVisible(WindowInsets.Type.ime())).thenReturn(true);
        mKeyboardInsetSupplier.set(0);
        verify(mKeyboardVisibilityListener).keyboardObscuringChanged(false);
    }

    @Test
    @EnableFeatures(BaseFeatures.SPLIT_SCREEN_IME_INSET_VISIBILITY)
    public void testImeVisibilitySupplier_NotifiesListenersWithoutLayout() {
        mKeyboardVisibilityDelegate.setKeyboardInsetSupplier(mKeyboardInsetSupplier);
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityListener);

        mImeVisibilitySupplier.set(true);
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(true);
        verify(mKeyboardVisibilityListener, never()).keyboardObscuringChanged(true);

        mImeVisibilitySupplier.set(false);
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(false);
    }

    @Test
    @DisableFeatures(BaseFeatures.SPLIT_SCREEN_IME_INSET_VISIBILITY)
    public void testImeVisibilitySupplier_featureDisabled_doesNotNotifyListenersWithoutLayout() {
        mKeyboardVisibilityDelegate.setKeyboardInsetSupplier(mKeyboardInsetSupplier);
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityListener);

        mImeVisibilitySupplier.set(true);
        verify(mKeyboardVisibilityListener, never()).keyboardVisibilityChanged(anyBoolean());
        verify(mKeyboardVisibilityListener, never()).keyboardObscuringChanged(anyBoolean());
    }

    @Test
    @EnableFeatures(BaseFeatures.SPLIT_SCREEN_IME_INSET_VISIBILITY)
    public void testKeyboardHiding_obscuringToHidden_noIntermediateNotification() {
        mKeyboardVisibilityDelegate.setKeyboardInsetSupplier(mKeyboardInsetSupplier);
        mKeyboardVisibilityDelegate.setImeVisibilitySupplier(mImeVisibilitySupplier);
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityListener);

        // Show obscuring keyboard
        setRootViewKeyboardInset(150);
        mKeyboardInsetSupplier.set(150);
        mImeVisibilitySupplier.set(true);
        verify(mKeyboardVisibilityListener).keyboardObscuringChanged(true);
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(true);
        clearInvocations(mKeyboardVisibilityListener);

        // Hide: InsetObserver sets mImeVisibilitySupplier to false first
        mImeVisibilitySupplier.set(false);
        // Verify no intermediate notification yet because inset is still 150
        verify(mKeyboardVisibilityListener, never()).keyboardVisibilityChanged(anyBoolean());
        verify(mKeyboardVisibilityListener, never()).keyboardObscuringChanged(anyBoolean());

        // Then InsetObserver updates keyboard inset to 0
        setRootViewKeyboardInset(0);
        mKeyboardInsetSupplier.set(0);
        verify(mKeyboardVisibilityListener).keyboardObscuringChanged(false);
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(false);
    }

    @Test
    @EnableFeatures(BaseFeatures.SPLIT_SCREEN_IME_INSET_VISIBILITY)
    public void testSetImeVisibilitySupplier_replacesObserverAndNotifiesWhenHidden() {
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityListener);
        assertTrue(mImeVisibilitySupplier.hasObservers());

        mImeVisibilitySupplier.set(true);
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(true);
        clearInvocations(mKeyboardVisibilityListener);

        SettableMonotonicObservableSupplier<Boolean> newSupplier =
                ObservableSuppliers.createMonotonic();
        mKeyboardVisibilityDelegate.setImeVisibilitySupplier(newSupplier);
        assertFalse(mImeVisibilitySupplier.hasObservers());
        assertTrue(newSupplier.hasObservers());
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(false);
    }

    @Test
    @EnableFeatures(BaseFeatures.SPLIT_SCREEN_IME_INSET_VISIBILITY)
    public void testIsKeyboardObscuring_zeroInsetsIgnoredWhenVisible() {
        mKeyboardVisibilityDelegate.setKeyboardInsetSupplier(mKeyboardInsetSupplier);
        mKeyboardVisibilityDelegate.addKeyboardVisibilityListener(mKeyboardVisibilityListener);

        setRootViewKeyboardInset(150);
        triggerLayout();
        verify(mKeyboardVisibilityListener).keyboardObscuringChanged(true);
        verify(mKeyboardVisibilityListener).keyboardVisibilityChanged(true);

        clearInvocations(mKeyboardVisibilityListener);

        when(mWindowInsets.getInsets(WindowInsets.Type.ime())).thenReturn(Insets.of(0, 0, 0, 0));
        when(mWindowInsets.isVisible(WindowInsets.Type.ime())).thenReturn(true);
        mKeyboardInsetSupplier.set(0);
        verify(mKeyboardVisibilityListener).keyboardObscuringChanged(false);
        verify(mKeyboardVisibilityListener, never()).keyboardVisibilityChanged(anyBoolean());
    }

    @Test
    @EnableFeatures(BaseFeatures.SPLIT_SCREEN_IME_INSET_VISIBILITY)
    public void testIsKeyboardVisible_otherWindowView_ignoresActivitySuppliers() {
        mImeVisibilitySupplier.set(true);

        // View belonging to another hierarchy/window
        View otherView = new View(ApplicationProvider.getApplicationContext());
        assertFalse(mKeyboardVisibilityDelegate.isKeyboardVisible(otherView));
        assertTrue(mKeyboardVisibilityDelegate.isKeyboardVisible(mRootView));

        mImeVisibilitySupplier.set(false);
        assertFalse(mKeyboardVisibilityDelegate.isKeyboardVisible(mRootView));
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
        when(mWindowInsets.isVisible(WindowInsets.Type.ime())).thenReturn(inset > 0);
    }
}
