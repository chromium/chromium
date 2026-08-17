// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.util.DisplayMetrics;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.PopupWindow;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Answers;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowView;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.R;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.AnchoredPopupWindow.SpecCalculator;
import org.chromium.ui.widget.AnchoredPopupWindow.VerticalOrientation;

/** Unit tests for {@link AnchoredPopupWindow}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = ShadowView.class)
public final class AnchoredPopupWindowTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    private Drawable mDrawable;
    @Mock private FrameLayout mContentView;
    @Mock private ChromePopupWindow mPopupWindow;
    @Mock private UiWidgetFactory mUiWidgetFactory;
    @Mock(answer = Answers.RETURNS_DEEP_STUBS)
    private View mView;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        mDrawable = new ColorDrawable(Color.RED);

        UiWidgetFactory.setInstance(mUiWidgetFactory);
        when(mUiWidgetFactory.createPopupWindow(any())).thenReturn(mPopupWindow);
        when(mPopupWindow.getBackground()).thenReturn(mock(Drawable.class));
        when(mPopupWindow.getContentView()).thenReturn(mContentView);

        when(mContentView.getMeasuredWidth()).thenReturn(500);
        when(mContentView.getMeasuredHeight()).thenReturn(500);

        DisplayMetrics fakeMetrics = new DisplayMetrics();
        fakeMetrics.density = 1;
        when(mView.getRootView().getResources().getDisplayMetrics()).thenReturn(fakeMetrics);
        when(mView.getRootView().isAttachedToWindow()).thenReturn(true);
    }

    @After
    public void tearDown() {
        mActivity.finish();
        UiWidgetFactory.setInstance(null);
    }

    @Test
    public void calculateAnimationStyleStartTop() {
        assertEquals(
                "Position below right -> animate from start top.",
                R.style.AnchoredPopupAnimStartTop,
                AnchoredPopupWindow.calculateAnimationStyle(
                        /* isPositionBelow= */ true, /* isPositionToLeft= */ false));
    }

    @Test
    public void calculateAnimationStyleStartBottom() {
        assertEquals(
                "Position above right -> animate from start bottom.",
                R.style.AnchoredPopupAnimStartBottom,
                AnchoredPopupWindow.calculateAnimationStyle(
                        /* isPositionBelow= */ false, /* isPositionToLeft= */ false));
    }

    @Test
    public void calculateAnimationStyleEndTop() {
        assertEquals(
                "Position below left -> animate from end top.",
                R.style.AnchoredPopupAnimEndTop,
                AnchoredPopupWindow.calculateAnimationStyle(
                        /* isPositionBelow= */ true, /* isPositionToLeft= */ true));
    }

    @Test
    public void calculateAnimationStyleEndBottom() {
        assertEquals(
                "Position above left -> animate from end bottom.",
                R.style.AnchoredPopupAnimEndBottom,
                AnchoredPopupWindow.calculateAnimationStyle(
                        /* isPositionBelow= */ false, /* isPositionToLeft= */ true));
    }

    @Test
    public void setAnimateFromAnchor() {
        AnchoredPopupWindow popupWindow =
                createAnchorPopupWindow(/* allowNonTouchableSize= */ true);
        popupWindow.setAnimateFromAnchor(true);
        popupWindow.showPopupWindow();
        verify(mPopupWindow).setAnimationStyle(anyInt());
    }

    @Test
    public void setInputMethodMode_delegatesToPopupWindow() {
        AnchoredPopupWindow popupWindow =
                createAnchorPopupWindow(/* allowNonTouchableSize= */ true);
        popupWindow.setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);
        verify(mPopupWindow).setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);
    }

    @Test
    public void setAnimationStyleNotOverrideByAnimateFromAnchor() {
        AnchoredPopupWindow popupWindow =
                createAnchorPopupWindow(/* allowNonTouchableSize= */ true);
        popupWindow.setAnimationStyle(R.style.DropdownPopupWindow);
        verify(mPopupWindow).setAnimationStyle(R.style.DropdownPopupWindow);

        popupWindow.setAnimateFromAnchor(true);
        popupWindow.showPopupWindow();
        // setAnimationStyle should only called once, since #setAnimateFromAnchor is no-op.
        verify(mPopupWindow, times(1)).setAnimationStyle(anyInt());
    }

    @Test
    public void testVerySmallPopupsDoNotShow() {
        when(mPopupWindow.isShowing()).thenReturn(false);
        when(mContentView.getMeasuredHeight()).thenReturn(1);
        when(mContentView.getMeasuredWidth()).thenReturn(1);

        AnchoredPopupWindow anchoredPopupWindow = createAnchorPopupWindow();
        anchoredPopupWindow.show();

        verify(mPopupWindow, never()).update(anyInt(), anyInt(), anyInt(), anyInt());
    }

    @Test
    public void testAllowVerySmallPopups() {
        when(mPopupWindow.isShowing()).thenReturn(false);
        when(mContentView.getMeasuredHeight()).thenReturn(1);
        when(mContentView.getMeasuredWidth()).thenReturn(1);

        AnchoredPopupWindow anchoredPopupWindow = createAnchorPopupWindow();
        anchoredPopupWindow.setAllowNonTouchableSize(true);
        anchoredPopupWindow.show();

        verify(mPopupWindow, times(1)).update(anyInt(), anyInt(), anyInt(), anyInt());
    }

    @Test
    public void testWebContentsRectChangesUpdatesPopup() {
        when(mPopupWindow.isShowing()).thenReturn(false);
        when(mContentView.getMeasuredHeight()).thenReturn(200);
        when(mContentView.getMeasuredWidth()).thenReturn(800);

        RectProvider anchorRectProvider = new RectProvider(new Rect(0, 0, 1000, 1000));
        RectProvider visibleWebContentsRectSupplier = new RectProvider(new Rect(0, 100, 1000, 900));
        AnchoredPopupWindow anchoredPopupWindow =
                new AnchoredPopupWindow(
                        mActivity,
                        mView,
                        mDrawable,
                        () -> mContentView,
                        anchorRectProvider,
                        visibleWebContentsRectSupplier);

        anchoredPopupWindow.show();

        verify(mPopupWindow, times(1)).update(anyInt(), anyInt(), anyInt(), anyInt());
        clearInvocations(mPopupWindow);

        // changing the rect should retrigger popup updates.
        visibleWebContentsRectSupplier.setRect(new Rect(0, 100, 1000, 500));

        verify(mPopupWindow, times(1)).update(anyInt(), anyInt(), anyInt(), anyInt());
    }

    // This is a temporary test that used to ensure the completeness of builder migraiton.
    @Test
    public void testBuilder() {
        RectProvider anchorRectProvider = new RectProvider(new Rect(0, 0, 1000, 1000));
        RectProvider viewportRectProvider = new RectProvider(new Rect(0, 100, 1000, 900));
        PopupWindow.OnDismissListener dismissListener = mock(PopupWindow.OnDismissListener.class);
        View.OnTouchListener touchListener = mock(View.OnTouchListener.class);
        AnchoredPopupWindow.LayoutObserver layoutObserver =
                mock(AnchoredPopupWindow.LayoutObserver.class);
        when(mPopupWindow.isFocusable()).thenReturn(true);
        when(mPopupWindow.getElevation()).thenReturn(20f);

        new AnchoredPopupWindow.Builder(
                        mActivity, mView, mDrawable, () -> mContentView, anchorRectProvider)
                .setViewportRectProvider(viewportRectProvider)
                .addOnDismissListener(dismissListener)
                .setTouchInterceptor(touchListener)
                .setLayoutObserver(layoutObserver)
                .setMargin(10)
                .setMaxWidth(200)
                .setDesiredContentSize(150, 300)
                .setPreferredVerticalOrientation(VerticalOrientation.ABOVE)
                .setPreferredHorizontalOrientation(HorizontalOrientation.CENTER)
                .setDismissOnTouchInteraction(true)
                .setVerticalOverlapAnchor(true)
                .setHorizontalOverlapAnchor(true)
                .setUpdateOrientationOnChange(true)
                .setSmartAnchorWithMaxWidth(true)
                .setAllowNonTouchableSize(true)
                .setAnimationStyle(R.style.DropdownPopupWindow)
                .setAnimateFromAnchor(true)
                .setFocusable(true)
                .setElevation(20f)
                .build();

        verify(mUiWidgetFactory).createPopupWindow(any());
    }

    @Test
    public void testCustomSpecCalculatorIsCalled() {
        RectProvider anchorRectProvider = new RectProvider(new Rect(0, 0, 100, 100));

        SpecCalculator mockCalculator = mock(SpecCalculator.class);
        // Return a valid PopupSpec to prevent NullPointerException
        AnchoredPopupWindow.PopupSpec fakeSpec =
                new AnchoredPopupWindow.PopupSpec(
                        new Rect(), mock(AnchoredPopupWindow.PopupPositionParams.class));
        when(mockCalculator.getPopupWindowSpec(
                        any(),
                        any(),
                        any(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyBoolean(),
                        anyBoolean(),
                        anyBoolean(),
                        anyBoolean(),
                        anyBoolean(),
                        anyBoolean()))
                .thenReturn(fakeSpec);

        AnchoredPopupWindow popupWindow =
                new AnchoredPopupWindow.Builder(
                                mActivity, mView, mDrawable, () -> mContentView, anchorRectProvider)
                        .setSpecCalculator(mockCalculator)
                        .build();

        popupWindow.show();

        verify(mockCalculator)
                .getPopupWindowSpec(
                        any(),
                        any(),
                        any(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyBoolean(),
                        anyBoolean(),
                        anyBoolean(),
                        anyBoolean(),
                        anyBoolean(),
                        anyBoolean());
    }

    @Test
    public void testPopupResizesOnRotationFromPortraitToLandscape() {
        when(mPopupWindow.isShowing()).thenReturn(false);
        when(mContentView.getMeasuredWidth()).thenReturn(800);
        when(mContentView.getMeasuredHeight()).thenReturn(200);

        // Initial portrait viewport (width: 400px, height: 1000px).
        RectProvider viewportRectProvider = new RectProvider(new Rect(0, 0, 400, 1000));
        AnchoredPopupWindow popupWindow =
                new AnchoredPopupWindow.Builder(
                                mActivity,
                                mView,
                                mDrawable,
                                () -> mContentView,
                                new RectProvider(new Rect(0, 0, 100, 100)))
                        .setViewportRectProvider(viewportRectProvider)
                        .setAllowNonTouchableSize(true)
                        .setUpdateOrientationOnChange(true)
                        .setSmartAnchorWithMaxWidth(true)
                        .setMaxWidth(1000)
                        .build();

        popupWindow.show();
        when(mPopupWindow.isShowing()).thenReturn(true);
        clearInvocations(mPopupWindow);

        // Rotate to landscape: Viewport width expands from 400px -> 800px.
        viewportRectProvider.setRect(new Rect(0, 0, 800, 600));
        verify(mPopupWindow).update(anyInt(), anyInt(), eq(800), anyInt());
    }

    @Test
    public void testSetInputMethodMode_builder() {
        new AnchoredPopupWindow.Builder(
                        mActivity,
                        mView,
                        mDrawable,
                        () -> mContentView,
                        new RectProvider(new Rect(0, 0, 100, 100)))
                .setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED)
                .build();

        verify(mPopupWindow).setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED);
    }

    private AnchoredPopupWindow createAnchorPopupWindow() {
        return createAnchorPopupWindow(/* allowNonTouchableSize= */ false);
    }

    private AnchoredPopupWindow createAnchorPopupWindow(boolean allowNonTouchableSize) {
        RectProvider provider = new RectProvider(new Rect(0, 0, 0, 0));
        AnchoredPopupWindow popup =
                new AnchoredPopupWindow(mActivity, mView, mDrawable, mContentView, provider);
        popup.setAllowNonTouchableSize(allowNonTouchableSize);
        return popup;
    }
}
