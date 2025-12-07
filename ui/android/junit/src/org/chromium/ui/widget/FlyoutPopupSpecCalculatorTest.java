// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.app.Activity;
import android.graphics.Rect;
import android.widget.FrameLayout;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowView;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.AnchoredPopupWindow.PopupSpec;
import org.chromium.ui.widget.AnchoredPopupWindow.VerticalOrientation;

/** Unit tests for the static positioning methods in {@link FlyoutPopupSpecCalculator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = ShadowView.class, qualifiers = "w600dp-h1000dp-mdpi")
public final class FlyoutPopupSpecCalculatorTest {
    private FlyoutPopupSpecCalculator mCalculator;

    private Rect mWindowRect;
    private int mRootWidth;
    private int mRootHeight;
    private int mPopupWidth;
    private int mPopupHeight;

    // Default settings for AnchoredPopup.
    private int mPaddingX;
    private int mPaddingY;
    private int mMarginPx;
    private int mMaxWidthPx;
    private int mDesiredWidthPx;
    private int mDesiredHeightPx;
    private @HorizontalOrientation int mPreferredHorizontalOrientation;
    private @VerticalOrientation int mPreferredVerticalOrientation;
    private boolean mCurrentPositionBelow;
    private boolean mCurrentPositionToLeft;
    private boolean mPreferCurrentOrientation;
    private boolean mHorizontalOverlapAnchor;
    private boolean mVerticalOverlapAnchor;
    private boolean mSmartAnchorWithMaxWidth;

    private FrameLayout mContentView;
    private Activity mActivity;

    @Before
    public void setUp() {
        mRootWidth = 600;
        mRootHeight = 1000;
        mPopupWidth = 150;
        mPopupHeight = 300;
        mWindowRect = new Rect(0, 0, mRootWidth, mRootHeight);

        mActivity = Robolectric.buildActivity(Activity.class).get();

        mContentView = new FrameLayout(mActivity);
        mContentView.setMinimumWidth(mPopupWidth);
        mContentView.setMinimumHeight(mPopupHeight);

        setDefaultValueForAnchoredPopup();

        mCalculator = new FlyoutPopupSpecCalculator();
    }

    @Test
    public void flyoutPopupSpecCalculatorTest_DefaultSettings() {
        // E.left = A.right = 100
        // E.top = A.top = 0
        // E.right = A.right + w = 250
        // E.bottom = A.top + h = 300
        doTestFlyoutAnchoredPopupAtRect(
                "Anchored to the right edge.",
                /* anchoredRect= */ new Rect(0, 0, 100, 50),
                /* expectedRect= */ new Rect(100, 0, 250, 300));

        // E.left = A.left - w = 400 - 150 = 250
        // E.top = A.top = 0
        // E.right = A.left = 400
        // E.bottom = A.top + h = 300
        doTestFlyoutAnchoredPopupAtRect(
                "Anchored to the left edge.",
                /* anchoredRect= */ new Rect(400, 0, 500, 50),
                /* expectedRect= */ new Rect(250, 0, 400, 300));
    }

    /**
     * Test cases for {@link FlyoutPopupSpecCalculator#calculatePopupWindowSpec}, calculation is
     * explained at each call site.
     */
    private void doTestFlyoutAnchoredPopupAtRect(
            String testCase, Rect anchoredRect, Rect expectedRect) {
        PopupSpec popupSpec =
                mCalculator.getPopupWindowSpec(
                        mWindowRect,
                        anchoredRect,
                        mContentView,
                        mRootWidth,
                        mPaddingX,
                        mPaddingY,
                        mMarginPx,
                        mMaxWidthPx,
                        mDesiredWidthPx,
                        mDesiredHeightPx,
                        mPreferredHorizontalOrientation,
                        mPreferredVerticalOrientation,
                        mCurrentPositionBelow,
                        mCurrentPositionToLeft,
                        mPreferCurrentOrientation,
                        mHorizontalOverlapAnchor,
                        mVerticalOverlapAnchor,
                        mSmartAnchorWithMaxWidth);

        Rect popupRect = popupSpec.popupRect;
        Assert.assertEquals(
                String.format("PopupRect does not match expected Rect. Test case:<%s>", testCase),
                expectedRect,
                popupRect);
    }

    private void setDefaultValueForAnchoredPopup() {
        mPaddingX = 0;
        mPaddingY = 0;
        mMarginPx = 0;
        mMaxWidthPx = 0;
        mDesiredWidthPx = 0;
        mDesiredHeightPx = 0;
        mPreferredHorizontalOrientation = HorizontalOrientation.MAX_AVAILABLE_SPACE;
        mPreferredVerticalOrientation = VerticalOrientation.MAX_AVAILABLE_SPACE;
        mCurrentPositionBelow = false;
        mCurrentPositionToLeft = false;
        mPreferCurrentOrientation = false;
        mHorizontalOverlapAnchor = false;
        mVerticalOverlapAnchor = false;
        mSmartAnchorWithMaxWidth = false;
    }
}
