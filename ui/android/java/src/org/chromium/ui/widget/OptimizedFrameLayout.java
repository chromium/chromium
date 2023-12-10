// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;

import java.util.ArrayList;
import java.util.List;

/**
 * This class overrides {@link FrameLayout#onMeasure} so that it does not call onMeasure on its
 * children multiple times during the same {@link FrameLayout#onMeasure} call.
 */
public class OptimizedFrameLayout extends FrameLayout {
    private static class MeasurementState {
        final View mView;
        final int mWidthMeasureSpec;
        final int mHeightMeasureSpec;

        MeasurementState(View view, int widthMeasureSpec, int heightMeasureSpec) {
            mView = view;
            mWidthMeasureSpec = widthMeasureSpec;
            mHeightMeasureSpec = heightMeasureSpec;
        }
    }

    private final List<MeasurementState> mMatchParentChildren = new ArrayList<>();

    public OptimizedFrameLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @SuppressLint("DrawAllocation")
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int count = getChildCount();

        final boolean measureMatchParentChildren =
                MeasureSpec.getMode(widthMeasureSpec) != MeasureSpec.EXACTLY
                        || MeasureSpec.getMode(heightMeasureSpec) != MeasureSpec.EXACTLY;
        mMatchParentChildren.clear();

        int maxHeight = 0;
        int maxWidth = 0;
        int childState = 0;

        int paddingLeft = getPaddingLeft();
        int paddingRight = getPaddingRight();
        int paddingTop = getPaddingTop();
        int paddingBottom = getPaddingBottom();

        for (int i = 0; i < count; i++) {
            final View child = getChildAt(i);
            if (getMeasureAllChildren() || child.getVisibility() != GONE) {
                final LayoutParams lp = (LayoutParams) child.getLayoutParams();

                final int childWidthMeasureSpec =
                        getChildMeasureSpec(
                                widthMeasureSpec,
                                paddingLeft + paddingRight + lp.leftMargin + lp.rightMargin,
                                lp.width);
                final int childHeightMeasureSpec =
                        getChildMeasureSpec(
                                heightMeasureSpec,
                                paddingTop + paddingBottom + lp.topMargin + lp.bottomMargin,
                                lp.height);

                child.measure(childWidthMeasureSpec, childHeightMeasureSpec);

                maxWidth =
                        Math.max(
                                maxWidth,
                                child.getMeasuredWidth() + lp.leftMargin + lp.rightMargin);
                maxHeight =
                        Math.max(
                                maxHeight,
                                child.getMeasuredHeight() + lp.topMargin + lp.bottomMargin);
                childState = combineMeasuredStates(childState, child.getMeasuredState());
                if (measureMatchParentChildren) {
                    if (lp.width == LayoutParams.MATCH_PARENT
                            || lp.height == LayoutParams.MATCH_PARENT) {
                        mMatchParentChildren.add(
                                new MeasurementState(
                                        child, childWidthMeasureSpec, childHeightMeasureSpec));
                    }
                }
            }
        }

        // Account for padding too
        maxWidth += paddingLeft + paddingRight;
        maxHeight += paddingTop + paddingBottom;

        // Check against our minimum height and width
        maxHeight = Math.max(maxHeight, getSuggestedMinimumHeight());
        maxWidth = Math.max(maxWidth, getSuggestedMinimumWidth());

        // Check against our foreground's minimum height and width
        final Drawable drawable = getForeground();
        if (drawable != null) {
            maxHeight = Math.max(maxHeight, drawable.getMinimumHeight());
            maxWidth = Math.max(maxWidth, drawable.getMinimumWidth());
        }

        setMeasuredDimension(
                resolveSizeAndState(maxWidth, widthMeasureSpec, childState),
                resolveSizeAndState(
                        maxHeight, heightMeasureSpec, childState << MEASURED_HEIGHT_STATE_SHIFT));

        count = mMatchParentChildren.size();
        if (count > 1) {
            for (int i = 0; i < count; i++) {
                final MeasurementState measurementState = mMatchParentChildren.get(i);
                final View child = measurementState.mView;
                final MarginLayoutParams lp = (MarginLayoutParams) child.getLayoutParams();

                final int childWidthMeasureSpec;
                if (lp.width == LayoutParams.MATCH_PARENT) {
                    final int width =
                            Math.max(
                                    0,
                                    getMeasuredWidth()
                                            - paddingLeft
                                            - paddingRight
                                            - lp.leftMargin
                                            - lp.rightMargin);
                    childWidthMeasureSpec = MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY);
                } else {
                    childWidthMeasureSpec =
                            getChildMeasureSpec(
                                    widthMeasureSpec,
                                    paddingLeft + paddingRight + lp.leftMargin + lp.rightMargin,
                                    lp.width);
                }

                final int childHeightMeasureSpec;
                if (lp.height == LayoutParams.MATCH_PARENT) {
                    final int height =
                            Math.max(
                                    0,
                                    getMeasuredHeight()
                                            - paddingTop
                                            - paddingBottom
                                            - lp.topMargin
                                            - lp.bottomMargin);
                    childHeightMeasureSpec =
                            MeasureSpec.makeMeasureSpec(height, MeasureSpec.EXACTLY);
                } else {
                    childHeightMeasureSpec =
                            getChildMeasureSpec(
                                    heightMeasureSpec,
                                    paddingTop + paddingBottom + lp.topMargin + lp.bottomMargin,
                                    lp.height);
                }

                if (measurementState.mWidthMeasureSpec != childWidthMeasureSpec
                        || measurementState.mHeightMeasureSpec != childHeightMeasureSpec) {
                    child.measure(childWidthMeasureSpec, childHeightMeasureSpec);
                }
            }
        }
        mMatchParentChildren.clear();
    }
}
