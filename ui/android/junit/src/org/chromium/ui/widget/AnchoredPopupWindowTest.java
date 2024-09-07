// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.PopupWindow;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Answers;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowView;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.R;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.AnchoredPopupWindow.PopupSpec;
import org.chromium.ui.widget.AnchoredPopupWindow.VerticalOrientation;

/** Unit tests for the static positioning methods in {@link AnchoredPopupWindow}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = ShadowView.class, qualifiers = "w600dp-h1000dp-mdpi")
public final class AnchoredPopupWindowTest {
    private Rect mWindowRect;
    int mRootWidth;
    int mRootHeight;
    int mPopupWidth;
    int mPopupHeight;

    // Default settings for AnchoredPopup.
    private int mPaddingX;
    private int mPaddingY;
    private int mMarginPx;
    private int mMaxWidthPx;
    private int mDesiredWidthPx;
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
    }

    @After
    public void tearDown() {
        mActivity.finish();
        UiWidgetFactory.setInstance(null);
    }

    @Test
    public void testGetPopupPosition_BelowRight() {
        Rect anchorRect = new Rect(10, 10, 20, 20);

        int spaceLeftOfAnchor =
                AnchoredPopupWindow.getSpaceLeftOfAnchor(anchorRect, mWindowRect, false);
        int spaceRightOfAnchor =
                AnchoredPopupWindow.getSpaceRightOfAnchor(anchorRect, mWindowRect, false);
        boolean positionToLeft =
                AnchoredPopupWindow.shouldPositionLeftOfAnchor(
                        spaceLeftOfAnchor, spaceRightOfAnchor, mPopupWidth, false, false);

        assertEquals("Space left of anchor incorrect.", 10, spaceLeftOfAnchor);
        assertEquals("Space right of anchor incorrect.", 580, spaceRightOfAnchor);
        assertFalse("positionToLeft incorrect.", positionToLeft);

        int x =
                AnchoredPopupWindow.getPopupX(
                        anchorRect,
                        mWindowRect,
                        mPopupWidth,
                        0,
                        false,
                        AnchoredPopupWindow.HorizontalOrientation.MAX_AVAILABLE_SPACE,
                        false);
        int y = AnchoredPopupWindow.getPopupY(anchorRect, mPopupHeight, false, true);

        assertEquals("Wrong x position.", 20, x);
        assertEquals("Wrong y position.", 20, y);
    }

    @Test
    public void testGetPopupPosition_BelowRight_Overlap() {
        Rect anchorRect = new Rect(10, 10, 20, 20);

        int spaceLeftOfAnchor =
                AnchoredPopupWindow.getSpaceLeftOfAnchor(anchorRect, mWindowRect, true);
        int spaceRightOfAnchor =
                AnchoredPopupWindow.getSpaceRightOfAnchor(anchorRect, mWindowRect, true);
        boolean positionToLeft =
                AnchoredPopupWindow.shouldPositionLeftOfAnchor(
                        spaceLeftOfAnchor, spaceRightOfAnchor, mPopupWidth, false, false);

        assertEquals("Space left of anchor incorrect.", 20, spaceLeftOfAnchor);
        assertEquals("Space right of anchor incorrect.", 590, spaceRightOfAnchor);
        assertFalse("positionToLeft incorrect.", positionToLeft);

        int x =
                AnchoredPopupWindow.getPopupX(
                        anchorRect,
                        mWindowRect,
                        mPopupWidth,
                        0,
                        true,
                        AnchoredPopupWindow.HorizontalOrientation.MAX_AVAILABLE_SPACE,
                        false);
        int y = AnchoredPopupWindow.getPopupY(anchorRect, mPopupHeight, true, true);

        assertEquals("Wrong x position.", 10, x);
        assertEquals("Wrong y position.", 10, y);
    }

    @Test
    public void testGetPopupPosition_BelowCenter() {
        Rect anchorRect = new Rect(295, 10, 305, 20);
        int x =
                AnchoredPopupWindow.getPopupX(
                        anchorRect,
                        mWindowRect,
                        mPopupWidth,
                        0,
                        false,
                        AnchoredPopupWindow.HorizontalOrientation.CENTER,
                        false);
        int y = AnchoredPopupWindow.getPopupY(anchorRect, mPopupHeight, false, true);

        assertEquals("Wrong x position.", 225, x);
        assertEquals("Wrong y position.", 20, y);
    }

    @Test
    public void getPopupPosition_AboveLeft() {
        Rect anchorRect = new Rect(400, 800, 410, 820);

        int spaceLeftOfAnchor =
                AnchoredPopupWindow.getSpaceLeftOfAnchor(anchorRect, mWindowRect, false);
        int spaceRightOfAnchor =
                AnchoredPopupWindow.getSpaceRightOfAnchor(anchorRect, mWindowRect, false);
        boolean positionToLeft =
                AnchoredPopupWindow.shouldPositionLeftOfAnchor(
                        spaceLeftOfAnchor, spaceRightOfAnchor, mPopupWidth, false, false);

        assertEquals("Space left of anchor incorrect.", 400, spaceLeftOfAnchor);
        assertEquals("Space right of anchor incorrect.", 190, spaceRightOfAnchor);
        assertTrue("positionToLeft incorrect.", positionToLeft);

        int x =
                AnchoredPopupWindow.getPopupX(
                        anchorRect,
                        mWindowRect,
                        mPopupWidth,
                        0,
                        false,
                        AnchoredPopupWindow.HorizontalOrientation.MAX_AVAILABLE_SPACE,
                        positionToLeft);
        int y = AnchoredPopupWindow.getPopupY(anchorRect, mPopupHeight, false, false);

        assertEquals("Wrong x position.", 250, x);
        assertEquals("Wrong y position.", 500, y);
    }

    @Test
    public void testGetPopupPosition_AboveLeft_Overlap() {
        Rect anchorRect = new Rect(400, 800, 410, 820);

        int spaceLeftOfAnchor =
                AnchoredPopupWindow.getSpaceLeftOfAnchor(anchorRect, mWindowRect, true);
        int spaceRightOfAnchor =
                AnchoredPopupWindow.getSpaceRightOfAnchor(anchorRect, mWindowRect, true);
        boolean positionToLeft =
                AnchoredPopupWindow.shouldPositionLeftOfAnchor(
                        spaceLeftOfAnchor, spaceRightOfAnchor, mPopupWidth, false, false);

        assertEquals("Space left of anchor incorrect.", 410, spaceLeftOfAnchor);
        assertEquals("Space right of anchor incorrect.", 200, spaceRightOfAnchor);
        assertTrue("positionToLeft incorrect.", positionToLeft);

        int x =
                AnchoredPopupWindow.getPopupX(
                        anchorRect,
                        mWindowRect,
                        mPopupWidth,
                        0,
                        true,
                        AnchoredPopupWindow.HorizontalOrientation.MAX_AVAILABLE_SPACE,
                        true);
        int y = AnchoredPopupWindow.getPopupY(anchorRect, mPopupHeight, true, false);

        assertEquals("Wrong x position.", 260, x);
        assertEquals("Wrong y position.", 520, y);
    }

    @Test
    public void testGetPopupPosition_ClampedLeftEdge() {
        Rect anchorRect = new Rect(10, 10, 20, 20);
        int x =
                AnchoredPopupWindow.getPopupX(
                        anchorRect,
                        mWindowRect,
                        mPopupWidth,
                        20,
                        false,
                        AnchoredPopupWindow.HorizontalOrientation.MAX_AVAILABLE_SPACE,
                        true);

        assertEquals("Wrong x position.", 20, x);
    }

    @Test
    public void testGetPopupPosition_ClampedRightEdge() {
        Rect anchorRect = new Rect(590, 800, 600, 820);
        int x =
                AnchoredPopupWindow.getPopupX(
                        anchorRect,
                        mWindowRect,
                        mPopupWidth,
                        20,
                        false,
                        AnchoredPopupWindow.HorizontalOrientation.MAX_AVAILABLE_SPACE,
                        true);

        assertEquals("Wrong x position.", 430, x);
    }

    @Test
    public void testShouldPositionLeftOfAnchor() {
        Rect anchorRect = new Rect(300, 10, 310, 20);
        int spaceLeftOfAnchor =
                AnchoredPopupWindow.getSpaceLeftOfAnchor(anchorRect, mWindowRect, false);
        int spaceRightOfAnchor =
                AnchoredPopupWindow.getSpaceRightOfAnchor(anchorRect, mWindowRect, false);
        boolean positionToLeft =
                AnchoredPopupWindow.shouldPositionLeftOfAnchor(
                        spaceLeftOfAnchor, spaceRightOfAnchor, mPopupWidth, false, false);

        assertEquals("Space left of anchor incorrect.", 300, spaceLeftOfAnchor);
        assertEquals("Space right of anchor incorrect.", 290, spaceRightOfAnchor);
        assertTrue("Should be positioned to the left.", positionToLeft);

        anchorRect = new Rect(250, 10, 260, 20);
        spaceLeftOfAnchor =
                AnchoredPopupWindow.getSpaceLeftOfAnchor(anchorRect, mWindowRect, false);
        spaceRightOfAnchor =
                AnchoredPopupWindow.getSpaceRightOfAnchor(anchorRect, mWindowRect, false);
        positionToLeft =
                AnchoredPopupWindow.shouldPositionLeftOfAnchor(
                        spaceLeftOfAnchor, spaceRightOfAnchor, mPopupWidth, true, true);

        // There is more space to the right, but the popup will still fit to the left and should
        // be positioned to the left.
        assertEquals("Space left of anchor incorrect.", 250, spaceLeftOfAnchor);
        assertEquals("Space right of anchor incorrect.", 340, spaceRightOfAnchor);
        assertTrue("Should still be positioned to the left.", positionToLeft);
    }

    @Test
    public void testGetMaxContentWidth() {
        int maxWidth = AnchoredPopupWindow.getMaxContentWidth(300, 600, 10, 10);
        assertEquals("Max width should be based on desired width.", 290, maxWidth);

        maxWidth = AnchoredPopupWindow.getMaxContentWidth(300, 300, 10, 10);
        assertEquals("Max width should be based on root view width.", 270, maxWidth);

        maxWidth = AnchoredPopupWindow.getMaxContentWidth(0, 600, 10, 10);
        assertEquals(
                "Max width should be based on root view width when desired with is 0.",
                570,
                maxWidth);

        maxWidth = AnchoredPopupWindow.getMaxContentWidth(300, 300, 10, 300);
        assertEquals("Max width should be clamped at 0.", 0, maxWidth);
    }

    // Test cases using #doTestAnchoredPopupAtRect.
    // All the test cases are explained with the abbreviation:
    //  anchorRect => A, expectedPopupRect => E, width = w, height = h

    @Test
    public void testCalcPopupRect_DefaultSettings() {
        // E.left = A.right = 0
        // E.top = A.bottom = 0
        // E.right = A.right + w = 150
        // E.bottom = A.bottom + h = 300
        doTestAnchoredPopupAtRect(
                "Anchored on bottom right.",
                /*anchorRect*/ new Rect(0, 0, 0, 0),
                /*expectedPopupRect*/ new Rect(0, 0, 150, 300));

        // E.left = A.left-w = 500 - 150 = 350
        // E.top = A.bottom = 0
        // E.right = A.left = 500
        // E.bottom = A.bottom + h = 0 + 300
        doTestAnchoredPopupAtRect(
                "Anchored on bottom left.",
                /*anchorRect*/ new Rect(500, 0, 500, 0),
                /*expectedPopupRect*/ new Rect(350, 0, 500, 300));

        // E.left = A.right = 0
        // E.top = A.top - h = 800 - 300 = 500
        // E.right = A.right + w = 0 + 150
        doTestAnchoredPopupAtRect(
                "Anchored on top right.",
                /*anchorRect*/ new Rect(0, 800, 0, 800),
                /*expectedPopupRect*/ new Rect(0, 500, 150, 800));

        // E.left = A.left - w = 600 - 150 = 450
        // E.top = A.top - h = 1000 - 300 = 700
        // E.right = A.left = 600
        // E.bottom = A.top = 1000
        doTestAnchoredPopupAtRect(
                "Anchored on top left due to space limit.",
                /*anchorRect*/ new Rect(600, 1000, 600, 1000),
                /*expectedPopupRect*/ new Rect(450, 700, 600, 1000));
    }

    @Test
    public void testCalcPopupRect_BiasOnX() {
        // E.left = A.left - w = 300 - 150 = 150
        // E.top = A.bottom = 0
        // E.right = A.left = 300
        // E.bottom = A.bottom + h = 300
        doTestAnchoredPopupAtRect(
                "Bias left when space is equal.",
                /*anchorRect*/ new Rect(300, 0, 300, 0),
                /*expectedPopupRect*/ new Rect(150, 0, 300, 300));

        // E.left = A.left - w = 200 - 150 = 50
        // E.top = A.top = 0
        // E.right = A.left = 200
        // E.bottom = A.bottom + h = 300
        doTestAnchoredPopupAtRect(
                "Bias left when it has more space.",
                /*anchorRect*/ new Rect(200, 0, 450, 0),
                /*expectedPopupRect*/ new Rect(50, 0, 200, 300));

        // E.left = A.right = 300
        // E.top = A.bottom = 0
        // E.right = A.right + w = 450
        // E.bottom = A.bottom + h = 300
        doTestAnchoredPopupAtRect(
                "Bias right when it has more space.",
                /*anchorRect*/ new Rect(150, 0, 300, 0),
                /*expectedPopupRect*/ new Rect(300, 0, 450, 300));
    }

    @Test
    public void testCalcPopupRect_BiasOnY() {
        // E.left = A.right = 0
        // E.top = A.bottom = 500
        // E.right = A.right + w = 150
        // E.bottom = A.bottom + h = 500 + 300 = 800
        doTestAnchoredPopupAtRect(
                "Bias below when space is equal.",
                /*anchorRect*/ new Rect(0, 500, 0, 500),
                /*expectedPopupRect*/ new Rect(0, 500, 150, 800));

        // E.left = A.right = 0
        // E.top = A.top - h = 600 - 300 = 300
        // E.right = A.right + w = 150
        // E.bottom = A.top = 600
        doTestAnchoredPopupAtRect(
                "Bias top when it has more space.",
                /*anchorRect*/ new Rect(0, 600, 0, 600),
                /*expectedPopupRect*/ new Rect(0, 300, 150, 600));

        // E.left = A.right = 0
        // E.top = A.bottom = 300
        // E.right = A.right + w = 150
        // E.bottom = A.bottom + h = 300 + 300 = 600
        doTestAnchoredPopupAtRect(
                "Bias below when it has more space",
                /*anchorRect*/ new Rect(0, 300, 0, 300),
                /*expectedPopupRect*/ new Rect(0, 300, 150, 600));
    }

    @Test
    public void testCalcPopupRect_LimitedSpaceX() {
        // When both left / right side does not have enough space, the anchor will have to overlap
        // with the anchor.
        // E.left = max(window.left, A.left - w) = max(0, 100 - 150) = 0
        // E.top = A.bottom = 0
        // E.right = E.left + w = 0 + 150 = 150
        // E.bottom = A.bottom + h = 300
        doTestAnchoredPopupAtRect(
                "Force overlap with anchor rect on left.",
                /*anchorRect*/ new Rect(100, 0, 600, 0),
                /*expectedPopupRect*/ new Rect(0, 0, 150, 300));

        // E.right = min(window.right, A.right + w) = min(600, 500 + 150) = 600
        // E.left = E.right - w = 600 - 150 = 450
        // E.top = A.bottom = 0
        // E.bottom = A.bottom + h = 300
        doTestAnchoredPopupAtRect(
                "Force overlap with anchor rect on right.",
                /*anchorRect*/ new Rect(0, 0, 500, 0),
                /*expectedPopupRect*/ new Rect(450, 0, 600, 300));
    }

    @Test
    public void testCalcPopupRect_LimitedSpaceY() {
        // Since mVerticalOverlapAnchor = false, even when space allows on the side, the popup
        // will not show on the side of anchor rect.
        // E.left = A.right = 0
        // E.top = A.bottom = 950
        // E.right = A.right + w = 150
        // E.bottom = min(window.bottom, A.bottom + h) = min(1000, 950+300) = 1000
        doTestAnchoredPopupAtRect(
                "Both above and below does not have enough space, anchored below due to bias. "
                        + "Reduce the height to fit into left over space on bottom.",
                /*anchorRect*/ new Rect(0, 100, 0, 950),
                /*expectedPopupRect*/ new Rect(0, 950, 150, 1000));
    }

    @Test
    public void testCalcPopupRect_Margin() {
        // TODO(crbug.com/40831293): Margin needs to be considered on Y axis.
        mMarginPx = 10;
        // E.left = A.right + margin = 0 + 10 = 10
        // E.top = A.bottom = 0
        // E.right = A.right + w + margin = 150 + 10 = 160
        // E.bottom = A.bottom + h = 300
        doTestAnchoredPopupAtRect(
                "Leave margin between screen and anchor rect.",
                /*anchorRect*/ new Rect(0, 0, 0, 0),
                /*expectedPopupRect*/ new Rect(10, 0, 160, 300));
    }

    @Test
    public void testCalcPopupRect_Padding() {
        mPaddingX = 3;
        mPaddingY = 2;
        // E.left = A.right = 0
        // E.top = A.bottom = 0
        // E.right = A.right + w + paddingX = 0 + 150 + 3 = 153
        // E.bottom = A.bottom + h + paddingY = 0 + 300 + 2 = 302
        doTestAnchoredPopupAtRect(
                "Adding padding into popup rect size.",
                /*anchorRect*/ new Rect(0, 0, 0, 0),
                /*expectedPopupRect*/ new Rect(0, 0, 153, 302));
    }

    @Test
    public void testCalcPopupRect_MaxWidth() {
        mMaxWidthPx = 200;
        // E.left = A.right = 0
        // E.top = A.bottom = 0
        // E.right = A.right + min(w, maxWidth) = 0 + min(150, 200) = 150
        // E.bottom = A.bottom + h = 300
        doTestAnchoredPopupAtRect(
                "Max width greater than expected size.",
                /*anchorRect*/ new Rect(0, 0, 0, 0),
                /*expectedPopupRect*/ new Rect(0, 0, 150, 300));

        mMaxWidthPx = 100;
        // E.left = A.right = 0
        // E.top = A.bottom = 0
        // E.right = A.right + min(w, maxWidth) = 0 + min(150, 100) = 100
        // E.bottom = A.bottom + h = 300
        doTestAnchoredPopupAtRect(
                "Max width limited to 100.",
                /*anchorRect*/ new Rect(0, 0, 0, 0),
                /*expectedPopupRect*/ new Rect(0, 0, 100, 300));
    }

    @Test
    public void testCalcPopupRect_DesiredWidth() {
        mDesiredWidthPx = 200;
        // E.left = A.right = 0
        // E.top = A.bottom = 0
        // E.right = A.right + desiredWidth = 0 + 200 = 200
        // E.bottom = A.bottom + h = 300
        doTestAnchoredPopupAtRect(
                "Popup shown as desired width.",
                /*anchorRect*/ new Rect(0, 0, 0, 0),
                /*expectedPopupRect*/ new Rect(0, 0, 200, 300));

        // E.left = max(window.left, A.left - desiredWidth) = max(0, 150-200) = 0
        // E.top = A.bottom = 0
        // E.right = E.left + desiredWidth = 200
        // E.bottom = A.bottom + h = 300
        doTestAnchoredPopupAtRect(
                "Shown as desired width even when available space is less.",
                /*anchorRect*/ new Rect(150, 0, 600, 0),
                /*expectedPopupRect*/ new Rect(0, 0, 200, 300));

        // E.left = A.right = 0
        // E.top = A.bottom = 0
        // E.right = A.right + min(maxWidth, desiredWidth) = 0 + min(180, 200) = 180
        // E.bottom = A.bottom + h = 300
        mMaxWidthPx = 180;
        doTestAnchoredPopupAtRect(
                "Desired width will respect a smaller max width.",
                /*anchorRect*/ new Rect(0, 0, 0, 0),
                /*expectedPopupRect*/ new Rect(0, 0, 180, 300));

        // E.left = A.right = 0
        // E.top = A.bottom = 0
        // E.right = A.right + min(maxWidth, desiredWidth) = 0 + min(300, 200) = 200
        // E.bottom = A.bottom + h = 300
        mMaxWidthPx = 300;
        doTestAnchoredPopupAtRect(
                "Popup shown as desired width when max width is larger.",
                /*anchorRect*/ new Rect(0, 0, 0, 0),
                /*expectedPopupRect*/ new Rect(0, 0, 200, 300));
    }

    @Test
    public void testCalcPopupRect_PreferredHorizontalOrientationCenter() {
        mPreferredHorizontalOrientation = HorizontalOrientation.CENTER;

        // E.left = (A.left + A.right) / 2 - w / 2 = (300 + 300) / 2 - 150 / 2 = 225
        // E.top = A.bottom = 0
        // E.right = E.left + w  = 225 + 150 = 375
        // E.bottom = A.bottom + h = 300
        doTestAnchoredPopupAtRect(
                "Below and center the anchor rect.",
                /*anchorRect*/ new Rect(300, 0, 300, 0),
                /*expectedPopupRect*/ new Rect(225, 0, 375, 300));

        // E.left = (A.left + A.right) / 2 - w / 2 = (200 + 400) / 2 - 150 / 2 = 225
        // E.top = A.bottom = 0
        // E.right = E.left + w  = 225 + 150 = 375
        // E.bottom = A.bottom + h = 300
        doTestAnchoredPopupAtRect(
                "Use the center of the anchor rect width.",
                /*anchorRect*/ new Rect(200, 0, 400, 0),
                /*expectedPopupRect*/ new Rect(225, 0, 375, 300));

        // E.left = (A.left + A.right) / 2 - w / 2 = (400 + 500) / 2 - 150 / 2 = 375
        // E.top = A.top - h = 600 - 300 = 300
        // E.right = E.left + w  = 225 + 150 = 525
        // E.bottom = A.top = 600
        doTestAnchoredPopupAtRect(
                "Above and center the anchor rect.",
                /*anchorRect*/ new Rect(400, 600, 500, 600),
                /*expectedPopupRect*/ new Rect(375, 300, 525, 600));
    }

    @Test
    public void testCalcPopupRect_PreferredHorizontalOrientationLayoutDirection() {
        mPreferredHorizontalOrientation = HorizontalOrientation.LAYOUT_DIRECTION;

        LocalizationUtils.setRtlForTesting(false);
        // E.left = A.left + w = 300
        // E.top = A.bottom = 0
        // E.right = E.left + w  = 300 + 150 = 450
        // E.bottom = A.bottom + h = 300
        doTestAnchoredPopupAtRect(
                "Right of anchor rect.",
                /*anchorRect*/ new Rect(300, 0, 300, 0),
                /*expectedPopupRect*/ new Rect(300, 0, 450, 300));

        LocalizationUtils.setRtlForTesting(true);
        // E.left = A.left - w = 300 - 150 = 150
        // E.top = A.bottom = 0
        // E.right = E.left = 300
        // E.bottom = A.bottom + h = 300
        doTestAnchoredPopupAtRect(
                "Left of anchor rect.",
                /*anchorRect*/ new Rect(300, 0, 300, 0),
                /*expectedPopupRect*/ new Rect(150, 0, 300, 300));
    }

    @Test
    public void testCalcPopupRect_PreferredVerticalOrientation() {
        mPreferredVerticalOrientation = VerticalOrientation.ABOVE;

        // E.left = A.right = 0
        // E.top = A.top - h = 300 - 300 = 0
        // E.right = A.right + w  = 150
        // E.bottom = A.top = 300
        doTestAnchoredPopupAtRect(
                "Show above the anchor even when bottom has more space.",
                /*anchorRect*/ new Rect(0, 300, 0, 300),
                /*expectedPopupRect*/ new Rect(0, 0, 150, 300));
        // E.left = A.right = 0
        // E.top = A.bottom = 200
        // E.right = A.right + w  = 150
        // E.bottom = A.bottom + h = 200 + 300 = 500
        doTestAnchoredPopupAtRect(
                "Show below the anchor since top does not have enough space.",
                /*anchorRect*/ new Rect(0, 200, 0, 200),
                /*expectedPopupRect*/ new Rect(0, 200, 150, 500));

        mPreferredVerticalOrientation = VerticalOrientation.BELOW;
        // E.left = A.right = 0
        // E.top = A.bottom = 600
        // E.right = A.right + w  = 150
        // E.bottom = A.bottom + h = 600 + 300 = 900
        doTestAnchoredPopupAtRect(
                "Show below the anchor even when top has more space.",
                /*anchorRect*/ new Rect(0, 600, 0, 600),
                /*expectedPopupRect*/ new Rect(0, 600, 150, 900));
        // E.left = A.right = 0
        // E.top = A.top - h = 800 - 300 = 500
        // E.right = A.right + w  = 150
        // E.bottom = A.top = 800
        doTestAnchoredPopupAtRect(
                "Show above the anchor since bottom does not have enough space.",
                /*anchorRect*/ new Rect(0, 800, 0, 800),
                /*expectedPopupRect*/ new Rect(0, 500, 150, 800));
    }

    @Test
    public void testCalcPopupRect_PreferCurrentOrientation() {
        mPreferCurrentOrientation = true;

        mCurrentPositionBelow = true;
        mCurrentPositionToLeft = true;

        // E.left = A.left - w = 200 - 150 = 50
        // E.top = A.bottom = 200
        // E.right = A.left = 200
        // E.bottom = A.bottom + h = 700 + 300 = 1000
        doTestAnchoredPopupAtRect(
                "Anchored left bottom as preferred current orientation.",
                /*anchorRect*/ new Rect(200, 700, 200, 700),
                /*expectedPopupRect*/ new Rect(50, 700, 200, 1000));
        // E.left = A.right = 200
        // E.top = A.top - h = 700 - 300 = 400
        // E.right = A.right + w  = 200 + 150 = 350
        // E.bottom = A.top = 700
        doTestAnchoredPopupAtRect(
                "Anchored top right due to limited space",
                /*anchorRect*/ new Rect(100, 700, 200, 800),
                /*expectedPopupRect*/ new Rect(200, 400, 350, 700));
    }

    @Test
    public void testCalcPopupRect_HorizontalOverlap() {
        mHorizontalOverlapAnchor = true;
        // E.left = A.left = 0
        // E.top = A.bottom = 200
        // E.right = A.left + w  = 0 + 150 = 150
        // E.bottom = A.bottom + h = 200 + 300 = 500
        doTestAnchoredPopupAtRect(
                "Horizontal overlap with rect while position to the right.",
                /*anchorRect*/ new Rect(0, 0, 100, 200),
                /*expectedPopupRect*/ new Rect(0, 200, 150, 500));
        // E.left = A.right - w = 600 - 150 = 450
        // E.top = A.bottom = 200
        // E.right = A.right = 600
        // E.bottom = A.bottom + h = 200 + 300 = 500
        doTestAnchoredPopupAtRect(
                "Horizontal overlap with rect while position to the left.",
                /*anchorRect*/ new Rect(400, 0, 600, 200),
                /*expectedPopupRect*/ new Rect(450, 200, 600, 500));
    }

    @Test
    public void testCalcPopupRect_VerticalOverlap() {
        mVerticalOverlapAnchor = true;
        // E.left = A.right = 100
        // E.top = A.top = 400
        // E.right = A.right + w = 100 + 150 = 250
        // E.bottom = A.top + h = 400 + 300 = 700
        doTestAnchoredPopupAtRect(
                "Vertical overlap with rect while position below.",
                /*anchorRect*/ new Rect(100, 400, 100, 600),
                /*expectedPopupRect*/ new Rect(100, 400, 250, 700));
        // E.left = A.right = 100
        // E.top = A.bottom - h = 900 - 300 = 600
        // E.right = A.right + w = 100 + 150 = 250
        // E.bottom = A.bottom = 900
        doTestAnchoredPopupAtRect(
                "Vertical overlap with rect while position below.",
                /*anchorRect*/ new Rect(100, 800, 100, 900),
                /*expectedPopupRect*/ new Rect(100, 600, 250, 900));
    }

    @Test
    public void testCalcPopupRect_SmartAnchorWithMaxWidth() {
        mHorizontalOverlapAnchor = false;
        mVerticalOverlapAnchor = true;
        // E.left = max(window.left, A.left - w) = max(0, 100-150) = 0
        // E.top = A.top = 200
        // E.right = E.left + w = 0 + 150 = 150
        // E.bottom = A.top + h = 200 + 300 = 500
        doTestAnchoredPopupAtRect(
                "Popup forced to horizontally overlap with anchor; "
                        + "vertical with anchor is expected.",
                /*anchorRect*/ new Rect(100, 200, 500, 800),
                /*expectedPopupRect*/ new Rect(0, 200, 150, 500));

        // Use smart anchor with max width to allow more width shown for the popup.
        mSmartAnchorWithMaxWidth = true;
        // E.left = A.right - w = 500 - 150 = 350
        // E.top = A.bottom = 800
        // E.right = A.right = 500
        // E.bottom = min(window.bottom, A.bottom + h) = min(1000, 800 + 300) = 1000
        doTestAnchoredPopupAtRect(
                "Popup adjusted to show below the anchored rect, "
                        + "while horizontally overlap with anchor but not vertically.",
                /*anchorRect*/ new Rect(100, 200, 500, 800),
                /*expectedPopupRect*/ new Rect(350, 800, 500, 1000));
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
                        null,
                        mContentView,
                        anchorRectProvider,
                        visibleWebContentsRectSupplier);

        anchoredPopupWindow.show();

        verify(mockPopup, times(1)).update(anyInt(), anyInt(), anyInt(), anyInt());
        clearInvocations(mockPopup);

        // changing the rect should retrigger popup updates.
        visibleWebContentsRectSupplier.setRect(new Rect(0, 100, 1000, 500));

        verify(mockPopup, times(1)).update(anyInt(), anyInt(), anyInt(), anyInt());
    }

    private void setDefaultValueForAnchoredPopup() {
        mPaddingX = 0;
        mPaddingY = 0;
        mMarginPx = 0;
        mMaxWidthPx = 0;
        mDesiredWidthPx = 0;
        mPreferredHorizontalOrientation = HorizontalOrientation.MAX_AVAILABLE_SPACE;
        mPreferredVerticalOrientation = VerticalOrientation.MAX_AVAILABLE_SPACE;
        mCurrentPositionBelow = false;
        mCurrentPositionToLeft = false;
        mPreferCurrentOrientation = false;
        mHorizontalOverlapAnchor = false;
        mVerticalOverlapAnchor = false;
        mSmartAnchorWithMaxWidth = false;
    }

    /**
     * Test cases for {@link AnchoredPopupWindow.calculatePopupWindowSpec}, calculation is explained
     * at each call site.
     */
    private void doTestAnchoredPopupAtRect(String testCase, Rect anchoredRect, Rect expectedRect) {
        PopupSpec popupSpec =
                AnchoredPopupWindow.calculatePopupWindowSpec(
                        mWindowRect,
                        anchoredRect,
                        mContentView,
                        mRootWidth,
                        mPaddingX,
                        mPaddingY,
                        mMarginPx,
                        mMaxWidthPx,
                        mDesiredWidthPx,
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

    private AnchoredPopupWindow createAnchorPopupWindow(int density) {
        View view = mock(View.class, Answers.RETURNS_DEEP_STUBS);
        DisplayMetrics fakeMetrics = new DisplayMetrics();
        fakeMetrics.density = density;
        when(view.getRootView().getResources().getDisplayMetrics()).thenReturn(fakeMetrics);
        when(view.getRootView().isAttachedToWindow()).thenReturn(true);
        RectProvider provider = new RectProvider(new Rect(0, 0, 0, 0));
        return new AnchoredPopupWindow(mActivity, view, null, mContentView, provider, null);
    }
}
