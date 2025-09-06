// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.graphics.Point;
import android.graphics.Rect;
import android.util.Size;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.AnchoredPopupWindow.PopupPositionParams;
import org.chromium.ui.widget.AnchoredPopupWindow.PopupSpec;
import org.chromium.ui.widget.AnchoredPopupWindow.SpecCalculator;
import org.chromium.ui.widget.AnchoredPopupWindow.VerticalOrientation;

/** Helper class that holds information and calculations for {@link AnchoredPopupWindow}. */
@NullMarked
public class PopupSpecCalculator implements SpecCalculator {

    @Override
    public PopupSpec getPopupWindowSpec(
            final Rect freeSpaceRect,
            final Rect anchorRect,
            final View contentView,
            final int rootViewWidth,
            int paddingX,
            int paddingY,
            int marginPx,
            int maxWidthPx,
            int desiredContentWidth,
            int desiredContentHeight,
            @HorizontalOrientation int preferredHorizontalOrientation,
            @VerticalOrientation int preferredVerticalOrientation,
            boolean currentPositionBelow,
            boolean currentPositionToLeft,
            boolean preferCurrentOrientation,
            boolean horizontalOverlapAnchor,
            boolean verticalOverlapAnchor,
            boolean smartAnchorWithMaxWidth) {
        return calculatePopupWindowSpec(
                freeSpaceRect,
                anchorRect,
                contentView,
                rootViewWidth,
                paddingX,
                paddingY,
                marginPx,
                maxWidthPx,
                desiredContentWidth,
                desiredContentHeight,
                preferredHorizontalOrientation,
                preferredVerticalOrientation,
                currentPositionBelow,
                currentPositionToLeft,
                preferCurrentOrientation,
                horizontalOverlapAnchor,
                verticalOverlapAnchor,
                smartAnchorWithMaxWidth);
    }

    @VisibleForTesting
    static PopupSpec calculatePopupWindowSpec(
            final Rect freeSpaceRect,
            final Rect anchorRect,
            final View contentView,
            final int rootViewWidth,
            int paddingX,
            int paddingY,
            int marginPx,
            int maxWidthPx,
            int desiredContentWidth,
            int desiredContentHeight,
            @HorizontalOrientation int preferredHorizontalOrientation,
            @VerticalOrientation int preferredVerticalOrientation,
            boolean currentPositionBelow,
            boolean currentPositionToLeft,
            boolean preferCurrentOrientation,
            boolean horizontalOverlapAnchor,
            boolean verticalOverlapAnchor,
            boolean smartAnchorWithMaxWidth) {
        final int maxContentWidth =
                getMaxContentWidth(maxWidthPx, rootViewWidth, marginPx, paddingX);

        final int widthSpec =
                desiredContentWidth > 0
                        ? MeasureSpec.makeMeasureSpec(
                                Math.min(desiredContentWidth, maxContentWidth), MeasureSpec.EXACTLY)
                        : MeasureSpec.makeMeasureSpec(maxContentWidth, MeasureSpec.AT_MOST);

        // Calculate the ideal content size.
        Size idealContentSize =
                calculateIdealContentSize(
                        contentView, widthSpec, desiredContentWidth, desiredContentHeight);

        // Calculate parameters and constraints.
        PopupPositionParams positionParams =
                calculatePositionParams(
                        horizontalOverlapAnchor,
                        verticalOverlapAnchor,
                        currentPositionBelow,
                        currentPositionToLeft,
                        preferCurrentOrientation,
                        smartAnchorWithMaxWidth,
                        preferredHorizontalOrientation,
                        preferredVerticalOrientation,
                        freeSpaceRect,
                        anchorRect,
                        idealContentSize,
                        paddingX,
                        paddingY,
                        marginPx,
                        maxContentWidth);

        // Decide the actual dimensions.
        Size size =
                calculatePopupSize(
                        contentView,
                        widthSpec,
                        desiredContentHeight,
                        positionParams.maxContentHeight,
                        paddingX,
                        paddingY);
        final Point popupPosition =
                calculatePopupPosition(
                        anchorRect,
                        freeSpaceRect,
                        size,
                        marginPx,
                        positionParams,
                        preferredHorizontalOrientation);

        int popupX = popupPosition.x;
        int popupY = popupPosition.y;

        return new PopupSpec(
                new Rect(popupX, popupY, popupX + size.getWidth(), popupY + size.getHeight()),
                positionParams);
    }

    private static Size calculateIdealContentSize(
            View contentView, int widthSpec, int desiredContentWidth, int desiredContentHeight) {
        int idealContentWidth;
        int idealContentHeight;
        if (desiredContentWidth > 0 && desiredContentHeight > 0) {
            idealContentWidth = desiredContentWidth;
            idealContentHeight = desiredContentHeight;
        } else {
            // If the desired content size is not fully specified, query the content view.
            final int queryHeightSpec =
                    desiredContentHeight > 0
                            ? MeasureSpec.makeMeasureSpec(desiredContentHeight, MeasureSpec.EXACTLY)
                            : MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
            contentView.measure(widthSpec, queryHeightSpec);
            idealContentWidth = contentView.getMeasuredWidth();
            idealContentHeight = contentView.getMeasuredHeight();
        }
        return new Size(idealContentWidth, idealContentHeight);
    }

    private static Size calculatePopupSize(
            View contentView,
            int widthSpec,
            int desiredContentHeight,
            int maxContentHeight,
            int paddingX,
            int paddingY) {
        final int heightSpec =
                desiredContentHeight > 0
                        ? MeasureSpec.makeMeasureSpec(
                                Math.min(desiredContentHeight, maxContentHeight),
                                MeasureSpec.EXACTLY)
                        : MeasureSpec.makeMeasureSpec(maxContentHeight, MeasureSpec.AT_MOST);
        contentView.measure(widthSpec, heightSpec);

        int width = contentView.getMeasuredWidth() + paddingX;
        int height = contentView.getMeasuredHeight() + paddingY;

        // Calculate the width of the contentView by adding the width of its children, their
        // margin,
        // and its own padding. This is necessary when a TextView overflows to multiple lines
        // because the contentView(parent) would return the maximum available width, which is
        // larger
        // than the actual needed width.
        ViewGroup parent = (ViewGroup) contentView;
        boolean isHorizontalLinearLayout =
                parent instanceof LinearLayout
                        && ((LinearLayout) parent).getOrientation() == LinearLayout.HORIZONTAL;
        if (isHorizontalLinearLayout && parent.getChildCount() > 0) {
            int contentMeasuredWidth = contentView.getPaddingStart() + contentView.getPaddingEnd();
            for (int index = 0; index < parent.getChildCount(); index++) {
                View childView = parent.getChildAt(index);
                int childWidth = childView.getMeasuredWidth();
                if (childWidth > 0) {
                    contentMeasuredWidth += childWidth;
                    LayoutParams lp = (LayoutParams) childView.getLayoutParams();
                    contentMeasuredWidth += lp.leftMargin + lp.rightMargin;
                }
            }
            width = contentMeasuredWidth + paddingX;
        }

        return new Size(width, height);
    }

    private static Point calculatePopupPosition(
            Rect anchorRect,
            Rect freeSpaceRect,
            Size size,
            int marginPx,
            PopupPositionParams positionParams,
            @HorizontalOrientation int preferredHorizontalOrientation) {
        // Determine the position of the text popup.
        final int popupX =
                getPopupX(
                        anchorRect,
                        freeSpaceRect,
                        size.getWidth(),
                        marginPx,
                        positionParams.allowHorizontalOverlap,
                        preferredHorizontalOrientation,
                        positionParams.isPositionToLeft);
        final int popupY =
                getPopupY(
                        anchorRect,
                        size.getHeight(),
                        positionParams.allowVerticalOverlap,
                        positionParams.isPositionBelow);
        return new Point(popupX, popupY);
    }

    @VisibleForTesting
    static int getMaxContentWidth(
            int desiredMaxWidthPx, int rootViewWidth, int marginPx, int paddingX) {
        int maxWidthBasedOnRootView = rootViewWidth - marginPx * 2;
        int maxWidth;
        if (desiredMaxWidthPx != 0 && desiredMaxWidthPx < maxWidthBasedOnRootView) {
            maxWidth = desiredMaxWidthPx;
        } else {
            maxWidth = maxWidthBasedOnRootView;
        }

        return maxWidth > paddingX ? maxWidth - paddingX : 0;
    }

    @VisibleForTesting
    static int getPopupX(
            Rect anchorRect,
            Rect windowRect,
            int popupWidth,
            int marginPx,
            boolean overlapAnchor,
            @HorizontalOrientation int horizontalOrientation,
            boolean positionToLeft) {
        int x;

        if (horizontalOrientation == HorizontalOrientation.CENTER) {
            x = anchorRect.left + (anchorRect.width() - popupWidth) / 2 + marginPx;
        } else if (positionToLeft) {
            x = (overlapAnchor ? anchorRect.right : anchorRect.left) - popupWidth;
        } else {
            x = overlapAnchor ? anchorRect.left : anchorRect.right;
        }

        // In landscape mode, root view includes the decorations in some devices. So we guard
        // the
        // window dimensions against |windowRect.right| instead.
        return clamp(x, marginPx, windowRect.right - popupWidth - marginPx);
    }

    // TODO(crbug.com/40831293): Account margin when position above the anchor.
    @VisibleForTesting
    static int getPopupY(
            Rect anchorRect, int popupHeight, boolean overlapAnchor, boolean positionBelow) {
        if (positionBelow) {
            return overlapAnchor ? anchorRect.top : anchorRect.bottom;
        } else {
            return (overlapAnchor ? anchorRect.bottom : anchorRect.top) - popupHeight;
        }
    }

    private static int clamp(int value, int a, int b) {
        int min = (a > b) ? b : a;
        int max = (a > b) ? a : b;
        if (value < min) {
            value = min;
        } else if (value > max) {
            value = max;
        }
        return value;
    }

    static PopupPositionParams calculatePositionParams(
            boolean horizontalOverlapAnchor,
            boolean verticalOverlapAnchor,
            boolean currentPositionBelow,
            boolean currentPositionToLeft,
            boolean preferCurrentOrientation,
            boolean smartAnchorWithMaxWidth,
            @HorizontalOrientation int preferredHorizontalOrientation,
            @VerticalOrientation int preferredVerticalOrientation,
            Rect freeSpaceRect,
            Rect anchorRect,
            Size idealContentSize,
            int paddingX,
            int paddingY,
            int marginPx,
            int maxContentWidth) {
        // Choose whether to place the popup, left or right of the anchor.
        boolean isPositionToLeft = currentPositionToLeft;
        boolean allowHorizontalOverlap = horizontalOverlapAnchor;
        boolean allowVerticalOverlap = verticalOverlapAnchor;
        if (preferredHorizontalOrientation == HorizontalOrientation.MAX_AVAILABLE_SPACE) {
            int spaceLeftOfAnchor =
                    getSpaceLeftOfAnchor(anchorRect, freeSpaceRect, allowHorizontalOverlap);
            int spaceRightOfAnchor =
                    getSpaceRightOfAnchor(anchorRect, freeSpaceRect, allowHorizontalOverlap);
            isPositionToLeft =
                    shouldPositionLeftOfAnchor(
                            spaceLeftOfAnchor,
                            spaceRightOfAnchor,
                            idealContentSize.getWidth() + paddingX + marginPx,
                            currentPositionToLeft,
                            preferCurrentOrientation);

            int idealWidthAroundAnchor = isPositionToLeft ? spaceLeftOfAnchor : spaceRightOfAnchor;
            if (idealWidthAroundAnchor < maxContentWidth && smartAnchorWithMaxWidth) {
                allowHorizontalOverlap = true;
                allowVerticalOverlap = false;
            }
        } else if (preferredHorizontalOrientation == HorizontalOrientation.LAYOUT_DIRECTION) {
            isPositionToLeft = LocalizationUtils.isLayoutRtl();
        }

        // Choose whether to place the popup, below or above the anchor.

        // TODO(dtrainor): This follows the previous logic.  But we should look into if we want
        // to
        // use the root view dimensions instead of the window dimensions here so the popup can't
        // bleed onto the decorations.
        final int spaceAboveAnchor =
                (allowVerticalOverlap ? anchorRect.bottom : anchorRect.top)
                        - freeSpaceRect.top
                        - paddingY
                        - marginPx;
        final int spaceBelowAnchor =
                freeSpaceRect.bottom
                        - (allowVerticalOverlap ? anchorRect.top : anchorRect.bottom)
                        - paddingY
                        - marginPx;

        // Bias based on the center of the popup and where it is on the screen.
        final boolean idealFitsBelow = idealContentSize.getHeight() <= spaceBelowAnchor;
        final boolean idealFitsAbove = idealContentSize.getHeight() <= spaceAboveAnchor;

        // Determine whether or not the popup should be above or below the anchor.
        // Aggressively try to put it below the anchor. Put it above only if it would fit
        // better.
        // TODO(crbug.com/40831291): Address cases where spaceBelowAnchor = 0, popup is still
        // biased to anchored below the rect.
        boolean isPositionBelow =
                (idealFitsBelow && spaceBelowAnchor >= spaceAboveAnchor) || !idealFitsAbove;

        // Override the ideal popup orientation if we are trying to maintain the current one.
        if (preferCurrentOrientation && currentPositionBelow != isPositionBelow) {
            if (currentPositionBelow && idealFitsBelow) isPositionBelow = true;
            if (!currentPositionBelow && idealFitsAbove) isPositionBelow = false;
        }

        if (preferredVerticalOrientation == VerticalOrientation.BELOW && idealFitsBelow) {
            isPositionBelow = true;
        }
        if (preferredVerticalOrientation == VerticalOrientation.ABOVE && idealFitsAbove) {
            isPositionBelow = false;
        }

        final int maxContentHeight = isPositionBelow ? spaceBelowAnchor : spaceAboveAnchor;

        return new PopupPositionParams(
                maxContentWidth,
                maxContentHeight,
                isPositionToLeft,
                isPositionBelow,
                allowHorizontalOverlap,
                allowVerticalOverlap);
    }

    @VisibleForTesting
    static int getSpaceLeftOfAnchor(Rect anchorRect, Rect windowRect, boolean overlapAnchor) {
        return (overlapAnchor ? anchorRect.right : anchorRect.left) - windowRect.left;
    }

    @VisibleForTesting
    static int getSpaceRightOfAnchor(Rect anchorRect, Rect windowRect, boolean overlapAnchor) {
        return windowRect.right - (overlapAnchor ? anchorRect.left : anchorRect.right);
    }

    @VisibleForTesting
    static boolean shouldPositionLeftOfAnchor(
            int spaceToLeftOfAnchor,
            int spaceToRightOfAnchor,
            int idealPopupWidth,
            boolean currentPositionToLeft,
            boolean preferCurrentOrientation) {
        boolean positionToLeft = spaceToLeftOfAnchor >= spaceToRightOfAnchor;

        // Override the ideal popup orientation if we are trying to maintain the current one.
        if (preferCurrentOrientation && positionToLeft != currentPositionToLeft) {
            if (currentPositionToLeft && idealPopupWidth <= spaceToLeftOfAnchor) {
                positionToLeft = true;
            }
            if (!currentPositionToLeft && idealPopupWidth <= spaceToRightOfAnchor) {
                positionToLeft = false;
            }
        }

        return positionToLeft;
    }
}
