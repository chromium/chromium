// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.graphics.Rect;
import android.util.Size;
import android.view.View;
import android.view.View.MeasureSpec;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.AnchoredPopupWindow.PopupPositionParams;
import org.chromium.ui.widget.AnchoredPopupWindow.PopupSpec;
import org.chromium.ui.widget.AnchoredPopupWindow.SpecCalculator;
import org.chromium.ui.widget.AnchoredPopupWindow.VerticalOrientation;

/** Helper class holds information of popup window (e.g. rect on screen, position to anchorRect) */
@NullMarked
public class FlyoutPopupSpecCalculator implements SpecCalculator {

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
        final int maxContentWidth =
                AnchoredPopupWindowUtils.getMaxContentWidth(
                        maxWidthPx, rootViewWidth, marginPx, paddingX);
        final int maxContentHeight =
                freeSpaceRect.bottom - freeSpaceRect.top - 2 * paddingY - 2 * marginPx;

        final int widthSpec =
                desiredContentWidth > 0
                        ? MeasureSpec.makeMeasureSpec(
                                Math.min(desiredContentWidth, maxContentWidth), MeasureSpec.EXACTLY)
                        : MeasureSpec.makeMeasureSpec(maxContentWidth, MeasureSpec.AT_MOST);

        // Calculate the ideal content size.
        Size idealContentSize =
                AnchoredPopupWindowUtils.calculateIdealContentSize(
                        contentView, widthSpec, desiredContentWidth, desiredContentHeight);

        // Decide whether the window should be displayed to the right or the left.
        int spaceLeftOfAnchor =
                AnchoredPopupWindowUtils.getSpaceLeftOfAnchor(
                        anchorRect, freeSpaceRect, /* overlapAnchor= */ false);
        int spaceRightOfAnchor =
                AnchoredPopupWindowUtils.getSpaceRightOfAnchor(
                        anchorRect, freeSpaceRect, /* overlapAnchor= */ false);

        boolean isPositionToLeft = LocalizationUtils.isLayoutRtl();

        int spaceInDefaultPosition = isPositionToLeft ? spaceLeftOfAnchor : spaceRightOfAnchor;
        int spaceInNonDefaultPosition = isPositionToLeft ? spaceRightOfAnchor : spaceLeftOfAnchor;
        if (spaceInDefaultPosition < idealContentSize.getWidth()
                && spaceInNonDefaultPosition
                        >= Math.min(idealContentSize.getWidth(), spaceInDefaultPosition)) {
            isPositionToLeft = !isPositionToLeft;
        }

        PopupPositionParams positionParams =
                new PopupPositionParams(
                        maxContentWidth,
                        maxContentHeight,
                        isPositionToLeft,
                        /* isPositionBelow= */ false,
                        /* allowHorizontalOverlap= */ false,
                        /* allowVerticalOverlap= */ true);

        Size size =
                AnchoredPopupWindowUtils.calculatePopupSize(
                        contentView,
                        widthSpec,
                        desiredContentHeight,
                        positionParams.maxContentHeight,
                        paddingX,
                        paddingY);

        final int popupX =
                AnchoredPopupWindowUtils.getPopupX(
                        anchorRect,
                        freeSpaceRect,
                        size.getWidth(),
                        marginPx,
                        /* overlapAnchor= */ false,
                        preferredHorizontalOrientation,
                        isPositionToLeft);

        // The first item in child popup should align with the parent item.
        final int popupY = anchorRect.top - contentView.getPaddingTop();

        return new PopupSpec(
                new Rect(popupX, popupY, popupX + size.getWidth(), popupY + size.getHeight()),
                positionParams);
    }
}
