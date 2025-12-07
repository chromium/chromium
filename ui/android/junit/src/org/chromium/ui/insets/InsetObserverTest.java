// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.insets;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Rect;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.view.WindowInsets;
import android.widget.LinearLayout;

import androidx.annotation.RequiresApi;
import androidx.core.graphics.Insets;
import androidx.core.view.DisplayCutoutCompat;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsAnimationCompat.BoundsCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.ui.base.ImmutableWeakReference;
import org.chromium.ui.insets.InsetObserver.WindowInsetsAnimationListener;
import org.chromium.ui.insets.InsetObserver.WindowInsetsConsumer;
import org.chromium.ui.insets.InsetObserver.WindowInsetsConsumer.InsetConsumerSource;

import java.util.Collections;

/** Tests for {@link InsetObserver} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class InsetObserverTest {
    /** The rect values if the display cutout is present. */
    private static final Rect DISPLAY_CUTOUT_RECT = new Rect(1, 1, 1, 1);

    /** The rect values if there is no cutout. */
    private static final Rect NO_CUTOUT_RECT = new Rect(0, 0, 0, 0);

    private static final Rect CUTOUT_WITH_PARTIAL_SYSTEM_INSET_RECT = new Rect(1, 0, 1, 0);

    /* Extra bottom inset that will be applied when e2e is enabled. */
    private static final int EDGE_TO_EDGE_BOTTOM_INSET = 2;

    /** The rect values if the display cutout is present in edge-to-edge mode. */
    private static final Rect E2E_DISPLAY_CUTOUT_RECT = new Rect(1, 1, 1, 3);

    /** The rect values if there is no cutout. */
    private static final Rect E2E_NO_CUTOUT_RECT = new Rect(0, 0, 0, 2);

    private static final Insets SYSTEM_BAR_INSETS = Insets.of(1, 1, 1, 1);
    private static final Insets SYSTEM_BAR_INSETS_PARTIAL = Insets.of(0, 1, 0, 1);

    private static final Insets SYSTEM_BAR_INSETS_MODIFIED = Insets.of(1, 1, 1, 2);

    private static final Insets SYSTEM_GESTURES_INSETS = Insets.of(1, 1, 1, 1);
    private static final Insets SYSTEM_GESTURES_INSETS_MODIFIED = Insets.of(1, 1, 1, 2);

    private static final Insets MANDATORY_SYSTEM_GESTURES_INSETS = Insets.of(0, 1, 0, 1);
    private static final Insets MANDATORY_SYSTEM_GESTURES_INSETS_MODIFIED = Insets.of(0, 1, 0, 2);

    private static final Insets NAVIGATION_BAR_INSETS = Insets.of(0, 0, 0, 1);
    private static final Insets NAVIGATION_BAR_INSETS_MODIFIED = Insets.of(0, 0, 0, 2);

    @Mock private InsetObserver.WindowInsetObserver mObserver;

    @Mock private WindowInsetsCompat mInsets;
    @Mock private WindowInsetsCompat mModifiedInsets;
    @Mock private WindowInsets mNonCompatInsets;
    @Mock private WindowInsets mModifiedNonCompatInsets;
    @Mock private WindowInsetsConsumer mInsetsConsumer1;
    @Mock private WindowInsetsConsumer mInsetsConsumer2;
    @Mock private WindowInsetsAnimationListener mInsetsAnimationListener;
    @Mock private LinearLayout mContentView;

    private InsetObserver mInsetObserver;

    private void setCutout(boolean hasCutout) {
        DisplayCutoutCompat cutout =
                hasCutout ? new DisplayCutoutCompat(new Rect(1, 1, 1, 1), null) : null;
        doReturn(cutout).when(mInsets).getDisplayCutout();
        doReturn(Insets.of(DISPLAY_CUTOUT_RECT))
                .when(mInsets)
                .getInsets(WindowInsetsCompat.Type.displayCutout());
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mNonCompatInsets).when(mInsets).toWindowInsets();
        doReturn(mModifiedNonCompatInsets).when(mModifiedInsets).toWindowInsets();
        doReturn(WindowInsetsCompat.CONSUMED.toWindowInsets())
                .when(mContentView)
                .onApplyWindowInsets(mNonCompatInsets);
        doReturn(WindowInsetsCompat.CONSUMED.toWindowInsets())
                .when(mContentView)
                .onApplyWindowInsets(mModifiedNonCompatInsets);

        doReturn(SYSTEM_BAR_INSETS).when(mInsets).getInsets(WindowInsetsCompat.Type.systemBars());
        doReturn(NAVIGATION_BAR_INSETS)
                .when(mInsets)
                .getInsets(WindowInsetsCompat.Type.navigationBars());
        doReturn(NAVIGATION_BAR_INSETS)
                .when(mInsets)
                .getInsetsIgnoringVisibility(WindowInsetsCompat.Type.navigationBars());

        doReturn(MANDATORY_SYSTEM_GESTURES_INSETS)
                .when(mInsets)
                .getInsets(WindowInsetsCompat.Type.mandatorySystemGestures());
        doReturn(MANDATORY_SYSTEM_GESTURES_INSETS_MODIFIED)
                .when(mModifiedInsets)
                .getInsets(WindowInsetsCompat.Type.mandatorySystemGestures());

        doReturn(SYSTEM_BAR_INSETS_MODIFIED)
                .when(mModifiedInsets)
                .getInsets(WindowInsetsCompat.Type.systemBars());
        doReturn(SYSTEM_GESTURES_INSETS)
                .when(mInsets)
                .getInsets(WindowInsetsCompat.Type.systemGestures());
        doReturn(SYSTEM_GESTURES_INSETS_MODIFIED)
                .when(mModifiedInsets)
                .getInsets(WindowInsetsCompat.Type.systemGestures());
        doReturn(NAVIGATION_BAR_INSETS_MODIFIED)
                .when(mModifiedInsets)
                .getInsets(WindowInsetsCompat.Type.navigationBars());
        doReturn(NAVIGATION_BAR_INSETS_MODIFIED)
                .when(mModifiedInsets)
                .getInsetsIgnoringVisibility(WindowInsetsCompat.Type.navigationBars());

        mInsetObserver =
                new InsetObserver(
                        new ImmutableWeakReference<>(mContentView),
                        /* enableKeyboardOverlayMode= */ true);
        mInsetObserver.addObserver(mObserver);
    }

    /** Test that applying new insets notifies observers. */
    @Test
    @SmallTest
    public void applyInsets_NotifiesObservers() {
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver, times(1)).onInsetChanged();
        verify(mObserver, times(1))
                .onSystemGestureInsetsChanged(
                        SYSTEM_GESTURES_INSETS.left,
                        SYSTEM_GESTURES_INSETS.top,
                        SYSTEM_GESTURES_INSETS.right,
                        SYSTEM_GESTURES_INSETS.bottom);
        clearInvocations(mObserver);

        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        // Apply the insets a second time; the insets observer should still be notified.
        verify(mObserver, times(1)).onInsetChanged();
        // The gesture insets observer should not be notified.
        verify(mObserver, never())
                .onSystemGestureInsetsChanged(anyInt(), anyInt(), anyInt(), anyInt());
        clearInvocations(mObserver);

        mInsetObserver.onApplyWindowInsets(mContentView, mModifiedInsets);
        verify(mObserver, times(1)).onInsetChanged();
        verify(mObserver, times(1))
                .onSystemGestureInsetsChanged(
                        SYSTEM_GESTURES_INSETS_MODIFIED.left,
                        SYSTEM_GESTURES_INSETS_MODIFIED.top,
                        SYSTEM_GESTURES_INSETS_MODIFIED.right,
                        SYSTEM_GESTURES_INSETS_MODIFIED.bottom);
    }

    @Test
    @SmallTest
    public void applyInsets_withInsetConsumer() {
        mInsetObserver.addInsetsConsumer(mInsetsConsumer1, InsetConsumerSource.TEST_SOURCE);

        doReturn(mModifiedInsets).when(mInsetsConsumer1).onApplyWindowInsets(mContentView, mInsets);
        doReturn(Insets.of(14, 17, 31, 43))
                .when(mModifiedInsets)
                .getInsets(WindowInsetsCompat.Type.systemBars());

        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mInsetsConsumer1).onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver, times(1)).onInsetChanged();
    }

    /** Test that consumed insets do not trigger observers. */
    @Test
    @SmallTest
    public void applyInsets_retriggerOnApplyWindowInsets() {
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver, times(1)).onInsetChanged();

        doReturn(mModifiedInsets).when(mInsetsConsumer1).onApplyWindowInsets(mContentView, mInsets);
        doReturn(Insets.NONE).when(mModifiedInsets).getInsets(anyInt());
        mInsetObserver.addInsetsConsumer(mInsetsConsumer1, InsetConsumerSource.TEST_SOURCE);
        mInsetObserver.retriggerOnApplyWindowInsets();

        // Observer should be notified by the consumer change.
        verify(mObserver, times(2)).onInsetChanged();
    }

    @Test
    @SmallTest
    public void applyInsets_withMultipleInsetConsumers() {
        // Add consumers in reverse order of priority.
        mInsetObserver.addInsetsConsumer(
                mInsetsConsumer1, InsetConsumerSource.APP_HEADER_COORDINATOR_BOTTOM);
        mInsetObserver.addInsetsConsumer(
                mInsetsConsumer2,
                InsetConsumerSource.DEFERRED_IME_WINDOW_INSET_APPLICATION_CALLBACK);

        // Assume that the higher priority |mInsetsConsumer2| consumes the system bars insets.
        doReturn(mModifiedInsets).when(mInsetsConsumer2).onApplyWindowInsets(mContentView, mInsets);
        doReturn(Insets.NONE).when(mModifiedInsets).getInsets(WindowInsetsCompat.Type.systemBars());
        doReturn(mModifiedInsets)
                .when(mInsetsConsumer1)
                .onApplyWindowInsets(mContentView, mModifiedInsets);

        // Verify that consumed insets are forwarded to the lower priority |mInsetsConsumer1|.
        var insets = mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        assertEquals(
                "System bar insets should be empty.",
                Insets.NONE,
                insets.getInsets(WindowInsetsCompat.Type.systemBars()));
        verify(mInsetsConsumer2).onApplyWindowInsets(mContentView, mInsets);
        verify(mInsetsConsumer1).onApplyWindowInsets(mContentView, mModifiedInsets);
    }

    @Test
    @SmallTest
    public void isKeyboardInOverlayMode() {
        mInsetObserver.setKeyboardInOverlayMode(true);
        assertTrue(mInsetObserver.isKeyboardInOverlayMode());

        mInsetObserver.setKeyboardInOverlayMode(false);
        assertFalse(mInsetObserver.isKeyboardInOverlayMode());
    }

    @Test
    @SmallTest
    public void insetAnimation() {
        mInsetObserver.addWindowInsetsAnimationListener(mInsetsAnimationListener);
        WindowInsetsAnimationCompat.Callback callback =
                mInsetObserver.getInsetAnimationProxyCallbackForTesting();
        WindowInsetsAnimationCompat animationCompat = new WindowInsetsAnimationCompat(8, null, 50);
        callback.onPrepare(animationCompat);
        verify(mInsetsAnimationListener).onPrepare(animationCompat);

        BoundsCompat bounds = new BoundsCompat(Insets.NONE, Insets.of(0, 0, 40, 40));
        callback.onStart(animationCompat, bounds);
        verify(mInsetsAnimationListener).onStart(animationCompat, bounds);

        WindowInsetsCompat insetsCompat = WindowInsetsCompat.CONSUMED;
        callback.onProgress(insetsCompat, Collections.emptyList());
        callback.onProgress(insetsCompat, Collections.emptyList());
        verify(mInsetsAnimationListener, times(2))
                .onProgress(insetsCompat, Collections.emptyList());

        callback.onEnd(animationCompat);
        verify(mInsetsAnimationListener).onEnd(animationCompat);
    }

    /** Test that applying new insets does not notify the observer. */
    @Test
    @SmallTest
    @RequiresApi(Build.VERSION_CODES.P)
    public void applyInsets() {
        setCutout(false);
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver, never()).onSafeAreaChanged(any());
    }

    /** Test that applying new insets with a cutout notifies the observer. */
    @Test
    @SmallTest
    @RequiresApi(Build.VERSION_CODES.P)
    public void applyInsets_WithCutout_WithSystemInsets() {
        setCutout(true);
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver, never()).onSafeAreaChanged(any());
    }

    /** Test that applying new insets with a cutout notifies the observer. */
    @Test
    public void applyInsets_WithCutout_NoSystemInsets() {
        setCutout(true);
        doReturn(Insets.NONE).when(mInsets).getInsets(WindowInsetsCompat.Type.systemBars());
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver).onSafeAreaChanged(DISPLAY_CUTOUT_RECT);
    }

    /** Test applying new insets with a cutout and then remove the cutout. */
    @Test
    @SmallTest
    @RequiresApi(Build.VERSION_CODES.P)
    public void applyInsets_WithCutout_ChangeWindowInsets() {
        setCutout(true);
        doReturn(Insets.NONE).when(mInsets).getInsets(WindowInsetsCompat.Type.systemBars());
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver).onSafeAreaChanged(DISPLAY_CUTOUT_RECT);

        reset(mObserver);
        doReturn(SYSTEM_BAR_INSETS).when(mInsets).getInsets(WindowInsetsCompat.Type.systemBars());
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver).onSafeAreaChanged(NO_CUTOUT_RECT);
    }

    /** Test applying new insets with a cutout and then remove the cutout. */
    @Test
    public void applyInsets_WithCutout_PartialSystemInsets() {
        setCutout(true);
        doReturn(SYSTEM_BAR_INSETS_PARTIAL)
                .when(mInsets)
                .getInsets(WindowInsetsCompat.Type.systemBars());
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver).onSafeAreaChanged(CUTOUT_WITH_PARTIAL_SYSTEM_INSET_RECT);
    }

    /** Test that applying new insets with a cutout but no observer is a no-op. */
    @Test
    @SmallTest
    @RequiresApi(Build.VERSION_CODES.P)
    public void applyInsets_WithCutout_NoListener() {
        setCutout(true);
        mInsetObserver.removeObserver(mObserver);
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
    }

    /** Test that applying new insets with no observer is a no-op. */
    @Test
    @SmallTest
    @RequiresApi(Build.VERSION_CODES.P)
    public void applyInsets_NoListener() {
        setCutout(false);
        mInsetObserver.removeObserver(mObserver);
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
    }

    @Test
    @RequiresApi(Build.VERSION_CODES.P)
    public void addEdgeToEdgeBottomInset() {
        setCutout(true);
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        mInsetObserver.updateBottomInsetForEdgeToEdge(EDGE_TO_EDGE_BOTTOM_INSET);
        verify(mObserver).onSafeAreaChanged(E2E_NO_CUTOUT_RECT);
        reset(mObserver);

        mInsetObserver.updateBottomInsetForEdgeToEdge(0);
        mInsetObserver.updateBottomInsetForEdgeToEdge(EDGE_TO_EDGE_BOTTOM_INSET);
        verify(mObserver).onSafeAreaChanged(NO_CUTOUT_RECT);
    }

    @Test
    @RequiresApi(Build.VERSION_CODES.P)
    public void addEdgeToEdgeBottomInset_NoSystemBars() {
        setCutout(true);
        doReturn(Insets.NONE).when(mInsets).getInsets(WindowInsetsCompat.Type.systemBars());
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        mInsetObserver.updateBottomInsetForEdgeToEdge(EDGE_TO_EDGE_BOTTOM_INSET);
        verify(mObserver).onSafeAreaChanged(E2E_DISPLAY_CUTOUT_RECT);
    }

    @Test
    @RequiresApi(Build.VERSION_CODES.P)
    public void addEdgeToEdgeBottomInset_NoCutout() {
        setCutout(false);
        mInsetObserver.updateBottomInsetForEdgeToEdge(EDGE_TO_EDGE_BOTTOM_INSET);
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver).onSafeAreaChanged(E2E_NO_CUTOUT_RECT);
    }

    @Test
    @RequiresApi(Build.VERSION_CODES.P)
    public void addEdgeToEdgeBottomInset_NoBottomInset() {
        setCutout(true);
        mInsetObserver.updateBottomInsetForEdgeToEdge(0);
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver, never()).onSafeAreaChanged(any());
    }

    @Test
    public void checkLastSeenRawWindowInsets() {
        assertNull(
                "WindowInsets does not have initial value.",
                mInsetObserver.getLastRawWindowInsets());

        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        assertEquals(
                "WindowInsets is different.", mInsets, mInsetObserver.getLastRawWindowInsets());

        mInsetObserver.onApplyWindowInsets(mContentView, mModifiedInsets);
        assertEquals(
                "WindowInsets is different.",
                mModifiedInsets,
                mInsetObserver.getLastRawWindowInsets());
    }

    @Test
    @Config(sdk = VERSION_CODES.R)
    public void initializeWithLastSeenRawWindowInsets() {
        doReturn(mNonCompatInsets).when(mContentView).getRootWindowInsets();
        mInsetObserver =
                new InsetObserver(
                        new ImmutableWeakReference<>(mContentView),
                        /* enableKeyboardOverlayMode= */ true);
        assertEquals(
                "WindowInsets is different.",
                WindowInsetsCompat.toWindowInsetsCompat(mNonCompatInsets),
                mInsetObserver.getLastRawWindowInsets());
    }

    @Test
    @SmallTest
    public void verifyInsets_gestureNav_recordsAppropriateHistograms() {
        mInsetObserver =
                new InsetObserver(
                        new ImmutableWeakReference<>(mContentView),
                        /* enableKeyboardOverlayMode= */ true);
        WindowInsetsCompat zeroNavbarInsets = mock(WindowInsetsCompat.class);
        doReturn(Insets.NONE)
                .when(zeroNavbarInsets)
                .getInsetsIgnoringVisibility(WindowInsetsCompat.Type.navigationBars());
        doReturn(Insets.of(0, 100, 0, 75))
                .when(zeroNavbarInsets)
                .getInsets(WindowInsetsCompat.Type.mandatorySystemGestures());
        doReturn(Insets.of(25, 100, 25, 75))
                .when(zeroNavbarInsets)
                .getInsets(WindowInsetsCompat.Type.systemGestures());
        doReturn(Insets.of(0, 100, 0, 0))
                .when(zeroNavbarInsets)
                .getInsets(WindowInsetsCompat.Type.tappableElement());

        WindowInsetsCompat nonZeroNavbarInsets = mock(WindowInsetsCompat.class);
        doReturn(Insets.of(0, 0, 0, 75))
                .when(nonZeroNavbarInsets)
                .getInsetsIgnoringVisibility(WindowInsetsCompat.Type.navigationBars());
        doReturn(Insets.of(0, 100, 0, 75))
                .when(nonZeroNavbarInsets)
                .getInsets(WindowInsetsCompat.Type.mandatorySystemGestures());
        doReturn(Insets.of(25, 100, 25, 75))
                .when(nonZeroNavbarInsets)
                .getInsets(WindowInsetsCompat.Type.systemGestures());
        doReturn(Insets.of(0, 100, 0, 0))
                .when(nonZeroNavbarInsets)
                .getInsets(WindowInsetsCompat.Type.tappableElement());

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.EdgeToEdge.NavigationBarMandatoryGesturesMismatch",
                                InsetObserver.HasSeenNonZeroNavBarState
                                        .GESTURE_NAV_FIRST_NAVBAR_MISSING)
                        .build();
        mInsetObserver.verifyInsetsForEdgeToEdge(zeroNavbarInsets);
        // Ensure duplicate calls don't result in multiple histograms.
        mInsetObserver.verifyInsetsForEdgeToEdge(zeroNavbarInsets);
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.EdgeToEdge.NavigationBarMandatoryGesturesMismatch",
                                InsetObserver.HasSeenNonZeroNavBarState.GESTURE_NAV_CORRECTION)
                        .build();
        mInsetObserver.verifyInsetsForEdgeToEdge(nonZeroNavbarInsets);
        // Ensure duplicate calls don't result in multiple histograms.
        mInsetObserver.verifyInsetsForEdgeToEdge(nonZeroNavbarInsets);
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.EdgeToEdge.NavigationBarMandatoryGesturesMismatch",
                                InsetObserver.HasSeenNonZeroNavBarState.GESTURE_NAV_REGRESSED)
                        .build();
        mInsetObserver.verifyInsetsForEdgeToEdge(zeroNavbarInsets);
        // Ensure duplicate calls don't result in multiple histograms.
        mInsetObserver.verifyInsetsForEdgeToEdge(zeroNavbarInsets);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void verifyInsets_tappableNav_recordsAppropriateHistograms() {
        mInsetObserver =
                new InsetObserver(
                        new ImmutableWeakReference<>(mContentView),
                        /* enableKeyboardOverlayMode= */ true);
        WindowInsetsCompat zeroNavbarInsets = mock(WindowInsetsCompat.class);
        doReturn(Insets.NONE)
                .when(zeroNavbarInsets)
                .getInsetsIgnoringVisibility(WindowInsetsCompat.Type.navigationBars());
        doReturn(Insets.of(0, 100, 0, 75))
                .when(zeroNavbarInsets)
                .getInsets(WindowInsetsCompat.Type.mandatorySystemGestures());
        doReturn(Insets.of(0, 100, 0, 75))
                .when(zeroNavbarInsets)
                .getInsets(WindowInsetsCompat.Type.systemGestures());
        doReturn(Insets.of(0, 100, 0, 75))
                .when(zeroNavbarInsets)
                .getInsets(WindowInsetsCompat.Type.tappableElement());

        WindowInsetsCompat nonZeroNavbarInsets = mock(WindowInsetsCompat.class);
        doReturn(Insets.of(0, 0, 0, 75))
                .when(nonZeroNavbarInsets)
                .getInsetsIgnoringVisibility(WindowInsetsCompat.Type.navigationBars());
        doReturn(Insets.of(0, 100, 0, 75))
                .when(nonZeroNavbarInsets)
                .getInsets(WindowInsetsCompat.Type.mandatorySystemGestures());
        doReturn(Insets.of(0, 100, 0, 75))
                .when(nonZeroNavbarInsets)
                .getInsets(WindowInsetsCompat.Type.systemGestures());
        doReturn(Insets.of(0, 100, 0, 75))
                .when(nonZeroNavbarInsets)
                .getInsets(WindowInsetsCompat.Type.tappableElement());

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.EdgeToEdge.NavigationBarMandatoryGesturesMismatch",
                                InsetObserver.HasSeenNonZeroNavBarState
                                        .TAPPABLE_NAV_FIRST_NAVBAR_MISSING)
                        .build();
        mInsetObserver.verifyInsetsForEdgeToEdge(zeroNavbarInsets);
        // Ensure duplicate calls don't result in multiple histograms.
        mInsetObserver.verifyInsetsForEdgeToEdge(zeroNavbarInsets);
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.EdgeToEdge.NavigationBarMandatoryGesturesMismatch",
                                InsetObserver.HasSeenNonZeroNavBarState.TAPPABLE_NAV_CORRECTION)
                        .build();
        mInsetObserver.verifyInsetsForEdgeToEdge(nonZeroNavbarInsets);
        // Ensure duplicate calls don't result in multiple histograms.
        mInsetObserver.verifyInsetsForEdgeToEdge(nonZeroNavbarInsets);
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.EdgeToEdge.NavigationBarMandatoryGesturesMismatch",
                                InsetObserver.HasSeenNonZeroNavBarState.TAPPABLE_NAV_REGRESSED)
                        .build();
        mInsetObserver.verifyInsetsForEdgeToEdge(zeroNavbarInsets);
        // Ensure duplicate calls don't result in multiple histograms.
        mInsetObserver.verifyInsetsForEdgeToEdge(zeroNavbarInsets);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void verifyInsets_bothNav_recordsAppropriateHistograms() {
        mInsetObserver =
                new InsetObserver(
                        new ImmutableWeakReference<>(mContentView),
                        /* enableKeyboardOverlayMode= */ true);
        WindowInsetsCompat zeroNavbarInsets = mock(WindowInsetsCompat.class);
        doReturn(Insets.NONE)
                .when(zeroNavbarInsets)
                .getInsetsIgnoringVisibility(WindowInsetsCompat.Type.navigationBars());
        doReturn(Insets.of(0, 100, 0, 75))
                .when(zeroNavbarInsets)
                .getInsets(WindowInsetsCompat.Type.mandatorySystemGestures());
        doReturn(Insets.of(25, 100, 25, 75))
                .when(zeroNavbarInsets)
                .getInsets(WindowInsetsCompat.Type.systemGestures());
        doReturn(Insets.of(0, 100, 0, 75))
                .when(zeroNavbarInsets)
                .getInsets(WindowInsetsCompat.Type.tappableElement());

        WindowInsetsCompat nonZeroNavbarInsets = mock(WindowInsetsCompat.class);
        doReturn(Insets.of(0, 0, 0, 75))
                .when(nonZeroNavbarInsets)
                .getInsetsIgnoringVisibility(WindowInsetsCompat.Type.navigationBars());
        doReturn(Insets.of(0, 100, 0, 75))
                .when(nonZeroNavbarInsets)
                .getInsets(WindowInsetsCompat.Type.mandatorySystemGestures());
        doReturn(Insets.of(25, 100, 25, 75))
                .when(nonZeroNavbarInsets)
                .getInsets(WindowInsetsCompat.Type.systemGestures());
        doReturn(Insets.of(0, 100, 0, 75))
                .when(nonZeroNavbarInsets)
                .getInsets(WindowInsetsCompat.Type.tappableElement());

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.EdgeToEdge.NavigationBarMandatoryGesturesMismatch",
                                InsetObserver.HasSeenNonZeroNavBarState
                                        .BOTH_NAV_FIRST_NAVBAR_MISSING)
                        .build();
        mInsetObserver.verifyInsetsForEdgeToEdge(zeroNavbarInsets);
        // Ensure duplicate calls don't result in multiple histograms.
        mInsetObserver.verifyInsetsForEdgeToEdge(zeroNavbarInsets);
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.EdgeToEdge.NavigationBarMandatoryGesturesMismatch",
                                InsetObserver.HasSeenNonZeroNavBarState.BOTH_NAV_CORRECTION)
                        .build();
        mInsetObserver.verifyInsetsForEdgeToEdge(nonZeroNavbarInsets);
        // Ensure duplicate calls don't result in multiple histograms.
        mInsetObserver.verifyInsetsForEdgeToEdge(nonZeroNavbarInsets);
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.EdgeToEdge.NavigationBarMandatoryGesturesMismatch",
                                InsetObserver.HasSeenNonZeroNavBarState.BOTH_NAV_REGRESSED)
                        .build();
        mInsetObserver.verifyInsetsForEdgeToEdge(zeroNavbarInsets);
        // Ensure duplicate calls don't result in multiple histograms.
        mInsetObserver.verifyInsetsForEdgeToEdge(zeroNavbarInsets);
        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    public void verifyInsets_neitherNav_recordsAppropriateHistograms() {
        mInsetObserver =
                new InsetObserver(
                        new ImmutableWeakReference<>(mContentView),
                        /* enableKeyboardOverlayMode= */ true);
        WindowInsetsCompat zeroNavbarInsets = mock(WindowInsetsCompat.class);
        doReturn(Insets.NONE)
                .when(zeroNavbarInsets)
                .getInsetsIgnoringVisibility(WindowInsetsCompat.Type.navigationBars());
        doReturn(Insets.of(0, 100, 0, 75))
                .when(zeroNavbarInsets)
                .getInsets(WindowInsetsCompat.Type.mandatorySystemGestures());
        doReturn(Insets.of(0, 100, 0, 75))
                .when(zeroNavbarInsets)
                .getInsets(WindowInsetsCompat.Type.systemGestures());
        doReturn(Insets.of(0, 100, 0, 0))
                .when(zeroNavbarInsets)
                .getInsets(WindowInsetsCompat.Type.tappableElement());

        WindowInsetsCompat nonZeroNavbarInsets = mock(WindowInsetsCompat.class);
        doReturn(Insets.of(0, 0, 0, 75))
                .when(nonZeroNavbarInsets)
                .getInsetsIgnoringVisibility(WindowInsetsCompat.Type.navigationBars());
        doReturn(Insets.of(0, 100, 0, 75))
                .when(nonZeroNavbarInsets)
                .getInsets(WindowInsetsCompat.Type.mandatorySystemGestures());
        doReturn(Insets.of(0, 100, 0, 75))
                .when(nonZeroNavbarInsets)
                .getInsets(WindowInsetsCompat.Type.systemGestures());
        doReturn(Insets.of(0, 100, 0, 0))
                .when(nonZeroNavbarInsets)
                .getInsets(WindowInsetsCompat.Type.tappableElement());

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.EdgeToEdge.NavigationBarMandatoryGesturesMismatch",
                                InsetObserver.HasSeenNonZeroNavBarState
                                        .NEITHER_NAV_FIRST_NAVBAR_MISSING)
                        .build();
        mInsetObserver.verifyInsetsForEdgeToEdge(zeroNavbarInsets);
        // Ensure duplicate calls don't result in multiple histograms.
        mInsetObserver.verifyInsetsForEdgeToEdge(zeroNavbarInsets);
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.EdgeToEdge.NavigationBarMandatoryGesturesMismatch",
                                InsetObserver.HasSeenNonZeroNavBarState.NEITHER_NAV_CORRECTION)
                        .build();
        mInsetObserver.verifyInsetsForEdgeToEdge(nonZeroNavbarInsets);
        // Ensure duplicate calls don't result in multiple histograms.
        mInsetObserver.verifyInsetsForEdgeToEdge(nonZeroNavbarInsets);
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.EdgeToEdge.NavigationBarMandatoryGesturesMismatch",
                                InsetObserver.HasSeenNonZeroNavBarState.NEITHER_NAV_REGRESSED)
                        .build();
        mInsetObserver.verifyInsetsForEdgeToEdge(zeroNavbarInsets);
        // Ensure duplicate calls don't result in multiple histograms.
        mInsetObserver.verifyInsetsForEdgeToEdge(zeroNavbarInsets);
        histogramWatcher.assertExpected();
    }
}
