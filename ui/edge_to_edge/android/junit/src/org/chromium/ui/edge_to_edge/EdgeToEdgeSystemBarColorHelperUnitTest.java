// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.edge_to_edge;

import static android.view.WindowInsetsController.APPEARANCE_LIGHT_NAVIGATION_BARS;
import static android.view.WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.graphics.Color;
import android.view.View;
import android.view.Window;
import android.view.WindowInsetsController;

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

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit test for {@link EdgeToEdgeSystemBarColorHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = 30)
public class EdgeToEdgeSystemBarColorHelperUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Window mWindow;
    @Mock private View mDecorView;
    @Mock private WindowInsetsController mWindowInsetsController;
    @Captor private ArgumentCaptor<Integer> mStatusBarAppearanceCaptor;
    @Captor private ArgumentCaptor<Integer> mNavigationBarAppearanceCaptor;
    @Mock private SystemBarColorHelper mDelegateColorHelper;

    private EdgeToEdgeSystemBarColorHelper mEdgeToEdgeColorHelper;
    private WindowSystemBarColorHelper mWindowHelper;
    private final ObservableSupplierImpl<Boolean> mShouldContentFitsWindowInsetsSupplier =
            new ObservableSupplierImpl<>();
    private final OneshotSupplierImpl<SystemBarColorHelper> mDelegateHelperSupplier =
            new OneshotSupplierImpl<>();

    @Before
    public void setup() {
        doReturn(mDecorView).when(mWindow).getDecorView();
        doReturn(mWindowInsetsController).when(mDecorView).getWindowInsetsController();
        doNothing()
                .when(mWindowInsetsController)
                .setSystemBarsAppearance(
                        mStatusBarAppearanceCaptor.capture(), eq(APPEARANCE_LIGHT_STATUS_BARS));
        doNothing()
                .when(mWindowInsetsController)
                .setSystemBarsAppearance(
                        mNavigationBarAppearanceCaptor.capture(),
                        eq(APPEARANCE_LIGHT_NAVIGATION_BARS));
        doReturn(true).when(mDelegateColorHelper).canSetStatusBarColor();
    }

    @Test
    public void initWhenNotEdgeToEdge_WithoutDelegateHelper_UseWindowHelper() {
        mShouldContentFitsWindowInsetsSupplier.set(true);
        initEdgeToEdgeColorHelper();

        mEdgeToEdgeColorHelper.setNavigationBarColor(Color.RED);
        verify(mWindow).setNavigationBarColor(Color.RED);

        mEdgeToEdgeColorHelper.setStatusBarColor(Color.RED);
        verify(mWindow).setStatusBarColor(Color.RED);
    }

    @Test
    public void initWhenNotEdgeToEdge_WithDelegateHelper_UseWindowHelper() {
        mShouldContentFitsWindowInsetsSupplier.set(true);
        mDelegateHelperSupplier.set(mDelegateColorHelper);
        initEdgeToEdgeColorHelper();

        mEdgeToEdgeColorHelper.setNavigationBarColor(Color.RED);
        verify(mWindow).setNavigationBarColor(Color.RED);

        mEdgeToEdgeColorHelper.setStatusBarColor(Color.RED);
        verify(mWindow).setStatusBarColor(Color.RED);
    }

    @Test
    public void initWhenEdgeToEdge_WithoutDelegateHelper_WindowTransparent() {
        mShouldContentFitsWindowInsetsSupplier.set(false);
        initEdgeToEdgeColorHelper();

        mEdgeToEdgeColorHelper.setNavigationBarColor(Color.RED);
        verify(mWindow, times(0)).setNavigationBarColor(Color.RED);

        // When delegateHelper is null, the window sets statusBarColor to mStatusBarColor.
        mEdgeToEdgeColorHelper.setStatusBarColor(Color.RED);
        verify(mWindow).setStatusBarColor(Color.RED);
    }

    @Test
    public void initWhenEdgeToEdge_WithDelegateHelper_UseDelegateHelper() {
        mShouldContentFitsWindowInsetsSupplier.set(false);
        mDelegateHelperSupplier.set(mDelegateColorHelper);
        initEdgeToEdgeColorHelper();

        mEdgeToEdgeColorHelper.setNavigationBarColor(Color.RED);
        verify(mDelegateColorHelper).setNavigationBarColor(Color.RED);

        mEdgeToEdgeColorHelper.setStatusBarColor(Color.RED);
        verify(mDelegateColorHelper).setStatusBarColor(Color.RED);
    }

    @Test
    public void setStatusBarColor_VerifyStatusBarAppearance() {
        initEdgeToEdgeColorHelper();

        mEdgeToEdgeColorHelper.setStatusBarColor(Color.WHITE);
        verifyStatusBarAppearance(/* isLight= */ true);
        mEdgeToEdgeColorHelper.setStatusBarColor(Color.BLACK);
        verifyStatusBarAppearance(/* isLight= */ false);
    }

    @Test
    public void setNavigationBarColor_VerifyNavigationBarAppearance() {
        initEdgeToEdgeColorHelper();

        mEdgeToEdgeColorHelper.setNavigationBarColor(Color.WHITE);
        verifyNavigationBarAppearance(/* isLight= */ true);
        mEdgeToEdgeColorHelper.setNavigationBarColor(Color.BLACK);
        verifyNavigationBarAppearance(/* isLight= */ false);
    }

    @Test
    public void switchIntoEdgeToEdge() {
        mDelegateHelperSupplier.set(mDelegateColorHelper);
        initEdgeToEdgeColorHelper();
        mEdgeToEdgeColorHelper.setNavigationBarColor(Color.RED);
        verify(mWindow).setNavigationBarColor(Color.RED);
        verify(mDelegateColorHelper, times(0)).setNavigationBarColor(anyInt());
        verify(mWindow).setNavigationBarContrastEnforced(true);
        verifyNavigationBarAppearance(/* isLight= */ false);
        clearInvocations(mDecorView);

        mEdgeToEdgeColorHelper.setStatusBarColor(Color.RED);
        verify(mDelegateColorHelper, times(0)).setStatusBarColor(anyInt());
        verify(mWindow).setStatusBarContrastEnforced(true);
        verifyStatusBarAppearance(/* isLight= */ false);

        clearInvocations(mWindow, mDecorView);
        doReturn(Color.RED).when(mWindow).getNavigationBarColor();
        doReturn(Color.RED).when(mWindow).getStatusBarColor();

        // Color will switch automatically when edge to edge mode changed.
        mShouldContentFitsWindowInsetsSupplier.set(false);
        verify(mDelegateColorHelper).setNavigationBarColor(Color.RED);
        verify(mDelegateColorHelper).setStatusBarColor(Color.RED);
        verify(mWindow).setNavigationBarColor(Color.TRANSPARENT);
        verify(mWindow).setStatusBarColor(Color.TRANSPARENT);
        verify(mWindow).setNavigationBarContrastEnforced(false);
        verify(mWindow).setStatusBarContrastEnforced(false);
        verifyStatusBarAppearance(/* isLight= */ false);
        verifyNavigationBarAppearance(/* isLight= */ false);
    }

    @Test
    public void switchOutFromEdgeToEdge() {
        mShouldContentFitsWindowInsetsSupplier.set(false);
        mDelegateHelperSupplier.set(mDelegateColorHelper);
        initEdgeToEdgeColorHelper();
        mEdgeToEdgeColorHelper.setNavigationBarColor(Color.RED);
        verify(mDelegateColorHelper).setNavigationBarColor(Color.RED);
        verify(mWindow, times(0)).setNavigationBarColor(Color.TRANSPARENT);
        verify(mWindow).setNavigationBarContrastEnforced(false);
        verifyNavigationBarAppearance(/* isLight= */ false);
        clearInvocations(mDecorView);

        mEdgeToEdgeColorHelper.setStatusBarColor(Color.RED);
        verify(mDelegateColorHelper).setStatusBarColor(Color.RED);
        verify(mWindow, times(0)).setStatusBarColor(Color.TRANSPARENT);
        verify(mWindow).setStatusBarContrastEnforced(false);
        verifyStatusBarAppearance(/* isLight= */ false);

        // Color will switch automatically when leaving edge to edge mode.
        clearInvocations(mDelegateColorHelper, mDecorView);
        mShouldContentFitsWindowInsetsSupplier.set(true);
        verify(mWindow).setNavigationBarColor(Color.RED);
        verify(mWindow).setStatusBarColor(Color.RED);
        verify(mDelegateColorHelper, times(0)).setNavigationBarColor(anyInt());
        verify(mWindow).setNavigationBarContrastEnforced(true);
        verify(mWindow).setStatusBarContrastEnforced(true);
        verifyStatusBarAppearance(/* isLight= */ false);
        verifyNavigationBarAppearance(/* isLight= */ false);
    }

    // A test case for #canSetStatusBarColor, and should be remove when all the method override
    // is removed.
    @Test
    public void delegateDoNotColorStatusBar() {
        doReturn(false).when(mDelegateColorHelper).canSetStatusBarColor();
        mShouldContentFitsWindowInsetsSupplier.set(false);
        mDelegateHelperSupplier.set(mDelegateColorHelper);
        initEdgeToEdgeColorHelper();

        mEdgeToEdgeColorHelper.setStatusBarColor(Color.RED);
        verify(mDelegateColorHelper, times(0)).setStatusBarColor(anyInt());
        verify(mWindow).setStatusBarColor(Color.RED);
        verifyStatusBarAppearance(/* isLight= */ false);
    }

    @Test
    public void initWhenNotEdgeToEdge_canColorStatusBarColorIsFalse() {
        mDelegateHelperSupplier.set(mDelegateColorHelper);
        mEdgeToEdgeColorHelper =
                new EdgeToEdgeSystemBarColorHelper(
                        mWindow,
                        mShouldContentFitsWindowInsetsSupplier,
                        mDelegateHelperSupplier,
                        /* canColorStatusBarColor= */ false);

        mEdgeToEdgeColorHelper.setStatusBarColor(Color.RED);
        // Status bar should not be colored when canColorStatusBarColor is false.
        verify(mWindow, never()).setStatusBarColor(anyInt());
        verify(mDelegateColorHelper, never()).setStatusBarColor(anyInt());
        verify(mWindowInsetsController, never()).setSystemBarsAppearance(anyInt(), anyInt());
    }

    private void initEdgeToEdgeColorHelper() {
        mEdgeToEdgeColorHelper =
                new EdgeToEdgeSystemBarColorHelper(
                        mWindow,
                        mShouldContentFitsWindowInsetsSupplier,
                        mDelegateHelperSupplier,
                        /* canColorStatusBarColor= */ true);
        mWindowHelper = mEdgeToEdgeColorHelper.getWindowHelperForTesting();
        clearInvocations(mDecorView);
    }

    private void verifyStatusBarAppearance(boolean isLight) {
        if (isLight) {
            assertEquals(
                    "The status bar should have a light appearance.",
                    APPEARANCE_LIGHT_STATUS_BARS,
                    (int) mStatusBarAppearanceCaptor.getValue());
        } else {
            assertEquals(
                    "The status bar should not have a light appearance.",
                    0,
                    (int) mStatusBarAppearanceCaptor.getValue());
        }
    }

    private void verifyNavigationBarAppearance(boolean isLight) {
        if (isLight) {
            assertEquals(
                    "The navigation bar should have a light appearance.",
                    APPEARANCE_LIGHT_NAVIGATION_BARS,
                    (int) mNavigationBarAppearanceCaptor.getValue());
        } else {
            assertEquals(
                    "The navigation bar should not have a light appearance.",
                    0,
                    (int) mNavigationBarAppearanceCaptor.getValue());
        }
    }
}
