// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
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
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.PopupWindow;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Answers;
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
    private FrameLayout mContentView;
    private Activity mActivity;
    private Drawable mDrawable;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).get();
        mDrawable = new ColorDrawable(Color.RED);
        mContentView = new FrameLayout(mActivity);
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
        // Set up for test case, so we have a mock popup window.
        UiWidgetFactory mockFactory = mock(UiWidgetFactory.class);
        UiWidgetFactory.setInstance(mockFactory);

        PopupWindow mockPopup = mock(PopupWindow.class);
        doReturn(mockPopup).when(mockFactory).createPopupWindow(any());

        AnchoredPopupWindow popupWindow = createAnchorPopupWindow(0);
        popupWindow.setAnimateFromAnchor(true);
        popupWindow.showPopupWindow();
        verify(mockPopup).setAnimationStyle(anyInt());
    }

    @Test
    public void setAnimationStyleNotOverrideByAnimateFromAnchor() {
        // Set up for test case, so we have a mock popup window.
        UiWidgetFactory mockFactory = mock(UiWidgetFactory.class);
        UiWidgetFactory.setInstance(mockFactory);
        PopupWindow mockPopup = mock(PopupWindow.class);
        doReturn(mockPopup).when(mockFactory).createPopupWindow(any());

        AnchoredPopupWindow popupWindow = createAnchorPopupWindow(0);
        popupWindow.setAnimationStyle(R.style.DropdownPopupWindow);
        verify(mockPopup).setAnimationStyle(R.style.DropdownPopupWindow);

        popupWindow.setAnimateFromAnchor(true);
        popupWindow.showPopupWindow();
        // setAnimationStyle should only called once, since #setAnimateFromAnchor is no-op.
        verify(mockPopup, times(1)).setAnimationStyle(anyInt());
    }

    @Test
    public void testVerySmallPopupsDoNotShow() {
        UiWidgetFactory mockFactory = mock(UiWidgetFactory.class);
        UiWidgetFactory.setInstance(mockFactory);
        PopupWindow mockPopup = mock(PopupWindow.class);
        when(mockPopup.isShowing()).thenReturn(false);
        when(mockPopup.getBackground()).thenReturn(mock(Drawable.class));
        when(mockFactory.createPopupWindow(any())).thenReturn(mockPopup);
        View contentView = mock(ViewGroup.class);
        when(contentView.getMeasuredHeight()).thenReturn(1);
        when(contentView.getMeasuredWidth()).thenReturn(1);
        when(mockPopup.getContentView()).thenReturn(contentView);

        AnchoredPopupWindow anchoredPopupWindow =
                createAnchorPopupWindow(DisplayMetrics.DENSITY_HIGH);
        anchoredPopupWindow.show();

        verify(mockPopup, never()).update(anyInt(), anyInt(), anyInt(), anyInt());
    }

    @Test
    public void testAllowVerySmallPopups() {
        UiWidgetFactory mockFactory = mock(UiWidgetFactory.class);
        UiWidgetFactory.setInstance(mockFactory);
        PopupWindow mockPopup = mock(PopupWindow.class);
        when(mockPopup.isShowing()).thenReturn(false);
        when(mockPopup.getBackground()).thenReturn(mock(Drawable.class));
        when(mockFactory.createPopupWindow(any())).thenReturn(mockPopup);
        View contentView = mock(ViewGroup.class);
        when(contentView.getMeasuredHeight()).thenReturn(1);
        when(contentView.getMeasuredWidth()).thenReturn(1);
        when(mockPopup.getContentView()).thenReturn(contentView);

        AnchoredPopupWindow anchoredPopupWindow =
                createAnchorPopupWindow(DisplayMetrics.DENSITY_HIGH);
        anchoredPopupWindow.setAllowNonTouchableSize(true);
        anchoredPopupWindow.show();

        verify(mockPopup, times(1)).update(anyInt(), anyInt(), anyInt(), anyInt());
    }

    @Test
    public void testWebContentsRectChangesUpdatesPopup() {
        UiWidgetFactory mockFactory = mock(UiWidgetFactory.class);
        UiWidgetFactory.setInstance(mockFactory);
        PopupWindow mockPopup = mock(PopupWindow.class);
        when(mockPopup.isShowing()).thenReturn(false);
        when(mockPopup.getBackground()).thenReturn(mock(Drawable.class));
        when(mockFactory.createPopupWindow(any())).thenReturn(mockPopup);
        View contentView = mock(ViewGroup.class);
        when(contentView.getMeasuredHeight()).thenReturn(200);
        when(contentView.getMeasuredWidth()).thenReturn(800);
        when(mockPopup.getContentView()).thenReturn(contentView);

        View view = mock(View.class, Answers.RETURNS_DEEP_STUBS);
        DisplayMetrics fakeMetrics = new DisplayMetrics();
        fakeMetrics.density = 1;
        when(view.getRootView().getResources().getDisplayMetrics()).thenReturn(fakeMetrics);
        when(view.getRootView().isAttachedToWindow()).thenReturn(true);
        RectProvider anchorRectProvider = new RectProvider(new Rect(0, 0, 1000, 1000));
        RectProvider visibleWebContentsRectSupplier = new RectProvider(new Rect(0, 100, 1000, 900));
        AnchoredPopupWindow anchoredPopupWindow =
                new AnchoredPopupWindow(
                        mActivity,
                        view,
                        mDrawable,
                        () -> contentView,
                        anchorRectProvider,
                        visibleWebContentsRectSupplier);

        anchoredPopupWindow.show();

        verify(mockPopup, times(1)).update(anyInt(), anyInt(), anyInt(), anyInt());
        clearInvocations(mockPopup);

        // changing the rect should retrigger popup updates.
        visibleWebContentsRectSupplier.setRect(new Rect(0, 100, 1000, 500));

        verify(mockPopup, times(1)).update(anyInt(), anyInt(), anyInt(), anyInt());
    }

    // This is a temporary test that used to ensure the completeness of builder migraiton.
    @Test
    public void testBuilder() {
        // Set up for test case, so we have a mock popup window.
        UiWidgetFactory mockFactory = mock(UiWidgetFactory.class);
        UiWidgetFactory.setInstance(mockFactory);
        PopupWindow mockPopup = mock(PopupWindow.class);
        doReturn(mockPopup).when(mockFactory).createPopupWindow(any());

        View view = mock(View.class, Answers.RETURNS_DEEP_STUBS);
        DisplayMetrics fakeMetrics = new DisplayMetrics();
        fakeMetrics.density = 1;
        when(view.getRootView().getResources().getDisplayMetrics()).thenReturn(fakeMetrics);
        when(view.getRootView().isAttachedToWindow()).thenReturn(true);
        RectProvider anchorRectProvider = new RectProvider(new Rect(0, 0, 1000, 1000));
        RectProvider viewportRectProvider = new RectProvider(new Rect(0, 100, 1000, 900));
        PopupWindow.OnDismissListener dismissListener = mock(PopupWindow.OnDismissListener.class);
        View.OnTouchListener touchListener = mock(View.OnTouchListener.class);
        AnchoredPopupWindow.LayoutObserver layoutObserver =
                mock(AnchoredPopupWindow.LayoutObserver.class);
        when(mockPopup.getContentView()).thenReturn(mContentView);
        when(mockPopup.isFocusable()).thenReturn(true);
        when(mockPopup.getElevation()).thenReturn(20f);

        new AnchoredPopupWindow.Builder(
                        mActivity, view, mDrawable, () -> mContentView, anchorRectProvider)
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

        verify(mockFactory).createPopupWindow(mActivity);
    }

    @Test
    public void testCustomSpecCalculatorIsCalled() {
        UiWidgetFactory mockFactory = mock(UiWidgetFactory.class);
        UiWidgetFactory.setInstance(mockFactory);
        PopupWindow mockPopup = mock(PopupWindow.class);
        when(mockFactory.createPopupWindow(any())).thenReturn(mockPopup);
        when(mockPopup.getBackground()).thenReturn(mock(Drawable.class));

        View view = mock(View.class, Answers.RETURNS_DEEP_STUBS);
        DisplayMetrics fakeMetrics = new DisplayMetrics();
        fakeMetrics.density = 1;
        when(view.getRootView().getResources().getDisplayMetrics()).thenReturn(fakeMetrics);
        when(view.getRootView().isAttachedToWindow()).thenReturn(true);
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
                                mActivity, view, mDrawable, () -> mContentView, anchorRectProvider)
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

    private AnchoredPopupWindow createAnchorPopupWindow(int density) {
        View view = mock(View.class, Answers.RETURNS_DEEP_STUBS);
        DisplayMetrics fakeMetrics = new DisplayMetrics();
        fakeMetrics.density = density;
        when(view.getRootView().getResources().getDisplayMetrics()).thenReturn(fakeMetrics);
        when(view.getRootView().isAttachedToWindow()).thenReturn(true);
        RectProvider provider = new RectProvider(new Rect(0, 0, 0, 0));
        return new AnchoredPopupWindow(mActivity, view, mDrawable, mContentView, provider);
    }
}
