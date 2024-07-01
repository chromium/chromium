// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.dragdrop;

import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapShader;
import android.graphics.Canvas;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.Point;
import android.graphics.RectF;
import android.graphics.Shader;
import android.media.ThumbnailUtils;
import android.util.FloatProperty;
import android.view.View;

import androidx.core.content.res.ResourcesCompat;

import org.chromium.ui.R;

/**
 * A {@link View.DragShadowBuilder} that animate the drag shadow from the original image in the web
 * to the center of the touch point. See go/animated-image-drag-shadow-corner-cases for known edge
 * cases.
 */
class AnimatedImageDragShadowBuilder extends View.DragShadowBuilder {
    /**
     * Animatable progress for the drag shadow. When the progress is 0, the drag shadow is full size
     * and the touch point of the original view matches the touch point in the shadow. As the
     * progress animates to 1, the drag shadow shrinks and translates to have the users finger in
     * the center of the drag shadow.
     */
    private final FloatProperty<AnimatedImageDragShadowBuilder> mProgressProperty =
            new FloatProperty<AnimatedImageDragShadowBuilder>("progress") {
                @Override
                public void setValue(
                        AnimatedImageDragShadowBuilder animatedImageDragShadowBuilder, float v) {
                    animatedImageDragShadowBuilder.mProgress = v;
                    animatedImageDragShadowBuilder.mContainerView.updateDragShadow(
                            animatedImageDragShadowBuilder);
                }

                @Override
                public Float get(AnimatedImageDragShadowBuilder animatedImageDragShadowBuilder) {
                    return animatedImageDragShadowBuilder.mProgress;
                }
            };

    private static final int ANIMATION_DURATION_MS = 300;
    private static final int MAX_ALPHA_VALUE = 255;
    private static final float TARGET_ALPHA_RATIO = 0.6f;

    private final float mEndCornerRadius;
    private final float mStartCornerRadius;
    private final Paint mPaint;
    private final Paint mPaintBorder;
    private final RectF mBitmapRect;
    private final Matrix mTransformMatrix;
    private final View mContainerView;
    private final int mBorderSize;

    private final RectF mStartBounds = new RectF();
    private final RectF mEndBounds = new RectF();
    private final RectF mCurrentBounds = new RectF();
    private final RectF mBorderBounds = new RectF();
    private final RectF mCanvasRect = new RectF();

    private float mProgress;

    /**
     * @param containerView The container view where the drag starts.
     * @param bitmap The bitmap which represents the shadow image.
     * @param startX The x offset of the touch point relative to the top left corner of the original
     *     image in the web.
     * @param startY The y offset of the touch point relative to the top left corner of the original
     *     image in the web.
     * @param dragShadowSpec The spec of the drag shadow including its size.
     */
    public AnimatedImageDragShadowBuilder(
            View containerView,
            Context context,
            Bitmap bitmap,
            float startX,
            float startY,
            DragShadowSpec dragShadowSpec) {
        this.mContainerView = containerView;
        Bitmap croppedDragShadow =
                ThumbnailUtils.extractThumbnail(
                        bitmap,
                        dragShadowSpec.startWidth,
                        dragShadowSpec.startHeight,
                        ThumbnailUtils.OPTIONS_RECYCLE_INPUT);
        float resizeRatio = (float) dragShadowSpec.targetWidth / dragShadowSpec.startWidth;
        Resources res = context.getResources();
        mProgress = 0f;

        mStartBounds.set(0, 0, dragShadowSpec.startWidth, dragShadowSpec.startHeight);

        // End bounds have a different scale, and centered at the users touch point.
        mEndBounds.set(0, 0, dragShadowSpec.targetWidth, dragShadowSpec.targetHeight);
        centerRectAtPoint(mEndBounds, startX, startY);

        // The canvas must include the start and end bounds to avoid clipping.
        mCanvasRect.set(mStartBounds);
        mCanvasRect.union(mEndBounds);

        // Reserve space for the border.
        mBorderSize = res.getDimensionPixelSize(R.dimen.drag_shadow_border_size);
        mCanvasRect.left -= mBorderSize;
        mCanvasRect.top -= mBorderSize;
        mCanvasRect.right += mBorderSize;
        mCanvasRect.bottom += mBorderSize;

        // Normalize everything into positive coordinates because we can't draw at negative
        // values on the canvas.
        float canvasOffsetX = -mCanvasRect.left;
        float canvasOffsetY = -mCanvasRect.top;
        mCanvasRect.offset(canvasOffsetX, canvasOffsetY);
        mStartBounds.offset(canvasOffsetX, canvasOffsetY);
        mEndBounds.offset(canvasOffsetX, canvasOffsetY);

        mEndCornerRadius = res.getDimensionPixelSize(R.dimen.drag_shadow_border_corner_radius);
        mStartCornerRadius = mEndCornerRadius / resizeRatio;

        mBitmapRect = new RectF(0, 0, croppedDragShadow.getWidth(), croppedDragShadow.getHeight());

        BitmapShader shader =
                new BitmapShader(croppedDragShadow, Shader.TileMode.CLAMP, Shader.TileMode.CLAMP);
        mPaint = new Paint();
        mPaint.setShader(shader);
        mPaintBorder = new Paint();
        mPaintBorder.setStyle(Paint.Style.STROKE);
        mPaintBorder.setStrokeWidth(mBorderSize);
        mPaintBorder.setColor(context.getColor(R.color.drag_shadow_outline_color));
        mTransformMatrix = new Matrix();
    }

    @Override
    public void onProvideShadowMetrics(Point outShadowSize, Point outShadowTouchPoint) {
        outShadowSize.set(Math.round(mCanvasRect.width()), Math.round(mCanvasRect.height()));
        outShadowTouchPoint.set(Math.round(mEndBounds.centerX()), Math.round(mEndBounds.centerY()));
        ObjectAnimator animator = ObjectAnimator.ofFloat(this, mProgressProperty, 1f);
        animator.setAutoCancel(true);
        animator.setDuration(ANIMATION_DURATION_MS);
        animator.start();
    }

    @Override
    public void onDrawShadow(Canvas canvas) {
        mCurrentBounds.set(
                lerp(mStartBounds.left, mEndBounds.left, mProgress),
                lerp(mStartBounds.top, mEndBounds.top, mProgress),
                lerp(mStartBounds.right, mEndBounds.right, mProgress),
                lerp(mStartBounds.bottom, mEndBounds.bottom, mProgress));

        mTransformMatrix.setRectToRect(mBitmapRect, mCurrentBounds, Matrix.ScaleToFit.CENTER);
        mPaint.getShader().setLocalMatrix(mTransformMatrix);

        float cornerRadius = lerp(mStartCornerRadius, mEndCornerRadius, mProgress);
        mPaint.setAlpha(Math.round(MAX_ALPHA_VALUE * (1f - mProgress * (1 - TARGET_ALPHA_RATIO))));
        canvas.drawRoundRect(mCurrentBounds, cornerRadius, cornerRadius, mPaint);
        // The border stroke is centered at the bounds. To avoid overlap with the shadow, the
        // stroke should be shifted outward half of the border size.
        mBorderBounds.set(
                mCurrentBounds.left - mBorderSize / 2f,
                mCurrentBounds.top - mBorderSize / 2f,
                mCurrentBounds.right + mBorderSize / 2f,
                mCurrentBounds.bottom + mBorderSize / 2f);
        canvas.drawRoundRect(mBorderBounds, cornerRadius, cornerRadius, mPaintBorder);
    }

    private static void centerRectAtPoint(RectF centerThisRect, float atX, float atY) {
        centerThisRect.offset(atX - centerThisRect.centerX(), atY - centerThisRect.centerY());
    }

    /** Linear interpolate from start value to stop value by amount [0..1] */
    private static float lerp(float start, float stop, float amount) {
        return start + (stop - start) * amount;
    }

    /**
     * Return the {@link DragShadowSpec} based on the image size and window size.
     * TODO(crbug.com/40214518): Scale image in C++ before passing into Java.
     */
    static DragShadowSpec getDragShadowSpec(
            Context context, int imageWidth, int imageHeight, int windowWidth, int windowHeight) {
        Resources resources = context.getResources();
        float startWidth = imageWidth;
        float startHeight = imageHeight;
        float targetWidth = startWidth;
        float targetHeight = startHeight;
        float truncatedWidth = 0f;
        float truncatedHeight = 0f;

        // Calculate the default scaled width / height.
        final float resizeRatio =
                ResourcesCompat.getFloat(resources, R.dimen.drag_shadow_resize_ratio);
        targetWidth *= resizeRatio;
        targetHeight *= resizeRatio;

        // Scale down if the width / height exceeded the max width / height, estimated based on the
        // window size, while maintaining the width-to-height ratio.
        final float maxSizeRatio =
                ResourcesCompat.getFloat(resources, R.dimen.drag_shadow_max_size_to_window_ratio);
        final float maxHeightPx = windowHeight * maxSizeRatio;
        final float maxWidthPx = windowWidth * maxSizeRatio;
        if (targetWidth > maxWidthPx || targetHeight > maxHeightPx) {
            final float downScaleRatio =
                    Math.min(maxHeightPx / targetHeight, maxWidthPx / targetWidth);
            targetWidth *= downScaleRatio;
            targetHeight *= downScaleRatio;
        }

        // Scale up with respect to the short side if the width or height is smaller than the min
        // size. Truncate the long side to stay within the max width / height as applicable.
        final int minSizePx = getDragShadowMinSize(resources);
        if (targetWidth <= targetHeight && targetWidth < minSizePx) {
            final float scaleUpRatio = minSizePx / targetWidth;
            targetHeight *= scaleUpRatio;
            targetWidth *= scaleUpRatio;
            if (targetHeight > maxHeightPx) {
                targetHeight = maxHeightPx;
                startHeight = targetHeight / targetWidth * startWidth;
                truncatedHeight = (imageHeight - startHeight) / 2;
            }
        } else if (targetHeight < minSizePx) {
            float scaleUpRatio = minSizePx / targetHeight;
            targetHeight *= scaleUpRatio;
            targetWidth *= scaleUpRatio;
            if (targetWidth > maxWidthPx) {
                targetWidth = maxWidthPx;
                startWidth = targetWidth / targetHeight * startHeight;
                truncatedWidth = (imageWidth - startWidth) / 2;
            }
        }
        return new DragShadowSpec(
                Math.round(startWidth),
                Math.round(startHeight),
                Math.round(targetWidth),
                Math.round(targetHeight),
                Math.round(truncatedWidth),
                Math.round(truncatedHeight));
    }

    /** Return the adjusted {@link CursorOffset} based on the drag object size and shadow size. */
    static CursorOffset adjustCursorOffset(
            float cursorOffsetX,
            float cursorOffsetY,
            int dragObjRectWidth,
            int dragObjRectHeight,
            DragShadowSpec dragShadowSpec) {
        assert dragShadowSpec.truncatedHeight == 0 || dragShadowSpec.truncatedWidth == 0
                : "Drag shadow should not be truncated in both dimensions";
        float adjustedOffsetX = cursorOffsetX;
        float adjustedOffsetY = cursorOffsetY;
        if (dragShadowSpec.truncatedHeight != 0) {
            float scaleFactor = (float) dragShadowSpec.startWidth / dragObjRectWidth;
            adjustedOffsetX *= scaleFactor;
            adjustedOffsetY *= scaleFactor;
            adjustedOffsetY -= dragShadowSpec.truncatedHeight;
            adjustedOffsetY = Math.max(0, adjustedOffsetY);
            adjustedOffsetY = Math.min(dragShadowSpec.startHeight, adjustedOffsetY);
            return new CursorOffset(adjustedOffsetX, adjustedOffsetY);
        }
        if (dragShadowSpec.truncatedWidth != 0) {
            float scaleFactor = (float) dragShadowSpec.startHeight / dragObjRectHeight;
            adjustedOffsetX *= scaleFactor;
            adjustedOffsetY *= scaleFactor;
            adjustedOffsetX -= dragShadowSpec.truncatedWidth;
            adjustedOffsetX = Math.max(0, adjustedOffsetX);
            adjustedOffsetX = Math.min(dragShadowSpec.startWidth, adjustedOffsetX);
            return new CursorOffset(adjustedOffsetX, adjustedOffsetY);
        }
        float scaleFactor = (float) dragShadowSpec.startWidth / dragObjRectWidth;
        adjustedOffsetX *= scaleFactor;
        adjustedOffsetY *= scaleFactor;
        return new CursorOffset(adjustedOffsetX, adjustedOffsetY);
    }

    /** Return the minimum size of the drag shadow image. */
    static int getDragShadowMinSize(Resources resources) {
        return resources.getDimensionPixelSize(R.dimen.drag_shadow_min_size);
    }

    static class DragShadowSpec {
        public final int startWidth;
        public final int startHeight;
        public final int targetWidth;
        public final int targetHeight;
        public final int truncatedWidth;
        public final int truncatedHeight;

        DragShadowSpec(
                int startWidth,
                int startHeight,
                int targetWidth,
                int targetHeight,
                int truncatedWidth,
                int truncatedHeight) {
            this.startWidth = startWidth;
            this.startHeight = startHeight;
            this.targetWidth = targetWidth;
            this.targetHeight = targetHeight;
            this.truncatedWidth = truncatedWidth;
            this.truncatedHeight = truncatedHeight;
        }
    }

    static class CursorOffset {
        public final float x;
        public final float y;

        CursorOffset(float x, float y) {
            this.x = x;
            this.y = y;
        }
    }
}
