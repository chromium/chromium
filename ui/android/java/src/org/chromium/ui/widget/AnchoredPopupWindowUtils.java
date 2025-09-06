// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.graphics.Rect;
import android.util.Size;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;

/**
 * A collection of static utility methods used for {@link AnchoredPopupWindow}'s positioning
 * calculations.
 */
@NullMarked
public class AnchoredPopupWindowUtils {
    static Size calculateIdealContentSize(
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

    static Size calculatePopupSize(
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
