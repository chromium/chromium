// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Rect;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.view.View;
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
import org.chromium.ui.InsetObserver.WindowInsetsAnimationListener;
import org.chromium.ui.InsetObserver.WindowInsetsConsumer;
import org.chromium.ui.base.ImmutableWeakReference;

import java.util.Collections;

/** Tests for {@link InsetObserver} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class InsetObserverTest {
    /** The rect values if the display cutout is present. */
    private static final Rect DISPLAY_CUTOUT_RECT = new Rect(1, 1, 1, 1);

    /** The rect values if there is no cutout. */
    private static final Rect NO_CUTOUT_RECT = new Rect(0, 0, 0, 0);

    /* Extra bottom inset that will be applied when e2e is enabled. */
    private static final int EDGE_TO_EDGE_BOTTOM_INSET = 2;

    /** The rect values if the display cutout is present in edge-to-edge mode. */
    private static final Rect E2E_DISPLAY_CUTOUT_RECT = new Rect(1, 1, 1, 3);

    /** The rect values if there is no cutout. */
    private static final Rect E2E_NO_CUTOUT_RECT = new Rect(0, 0, 0, 2);

    private static final Insets SYSTEM_BAR_INSETS = Insets.of(1, 1, 1, 1);

    private static final Insets SYSTEM_BAR_INSETS_MODIFIED = Insets.of(1, 1, 1, 2);

    @Mock private InsetObserver.WindowInsetObserver mObserver;

    @Mock private WindowInsetsCompat mInsets;
    @Mock private WindowInsetsCompat mModifiedInsets;
    @Mock private WindowInsets mNonCompatInsets;
    @Mock private WindowInsets mModifiedNonCompatInsets;
    @Mock private WindowInsetsConsumer mInsetsConsumer;
    @Mock private WindowInsetsAnimationListener mInsetsAnimationListener;
    @Mock private LinearLayout mContentView;

    private InsetObserver mInsetObserver;

    private void setCutout(boolean hasCutout) {
        DisplayCutoutCompat cutout =
                hasCutout ? new DisplayCutoutCompat(new Rect(1, 1, 1, 1), null) : null;
        doReturn(cutout).when(mInsets).getDisplayCutout();
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
        doReturn(SYSTEM_BAR_INSETS_MODIFIED)
                .when(mModifiedInsets)
                .getInsets(WindowInsetsCompat.Type.systemBars());

        mInsetObserver = new InsetObserver(new ImmutableWeakReference<View>(mContentView));
        mInsetObserver.addObserver(mObserver);
    }

    /** Test that applying new insets notifies observers. */
    @Test
    @SmallTest
    public void applyInsets_NotifiesObservers() {
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver, times(1)).onInsetChanged(1, 1, 1, 1);

        // Apply the insets a second time; the observer should not be notified.
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver, times(1)).onInsetChanged(1, 1, 1, 1);

        doReturn(Insets.of(1, 1, 1, 2))
                .when(mInsets)
                .getInsets(WindowInsetsCompat.Type.systemBars());
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver).onInsetChanged(1, 1, 1, 2);
    }

    @Test
    @SmallTest
    public void applyInsets_withInsetConsumer() {
        mInsetObserver.addInsetsConsumer(mInsetsConsumer);

        doReturn(mModifiedInsets).when(mInsetsConsumer).onApplyWindowInsets(mContentView, mInsets);
        doReturn(Insets.of(14, 17, 31, 43))
                .when(mModifiedInsets)
                .getInsets(WindowInsetsCompat.Type.systemBars());

        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mInsetsConsumer).onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver, times(1)).onInsetChanged(14, 17, 31, 43);
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
    public void applyInsets_WithCutout() {
        setCutout(true);
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver).onSafeAreaChanged(DISPLAY_CUTOUT_RECT);
    }

    /** Test applying new insets with a cutout and then remove the cutout. */
    @Test
    @SmallTest
    @RequiresApi(Build.VERSION_CODES.P)
    public void applyInsets_WithCutout_WithoutCutout() {
        setCutout(true);
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver).onSafeAreaChanged(DISPLAY_CUTOUT_RECT);

        reset(mObserver);
        setCutout(false);
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
        verify(mObserver).onSafeAreaChanged(NO_CUTOUT_RECT);
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
        mInsetObserver.updateBottomInsetForEdgeToEdge(EDGE_TO_EDGE_BOTTOM_INSET);
        verify(mObserver).onSafeAreaChanged(E2E_NO_CUTOUT_RECT);

        reset(mObserver);
        mInsetObserver.onApplyWindowInsets(mContentView, mInsets);
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
        verify(mObserver).onSafeAreaChanged(DISPLAY_CUTOUT_RECT);
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
        mInsetObserver = new InsetObserver(new ImmutableWeakReference<View>(mContentView));
        assertEquals(
                "WindowInsets is different.",
                WindowInsetsCompat.toWindowInsetsCompat(mNonCompatInsets),
                mInsetObserver.getLastRawWindowInsets());
    }
}
