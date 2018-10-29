// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.BitmapShader;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Shader;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.shapes.RoundRectShape;
import android.graphics.drawable.shapes.Shape;
import android.support.annotation.Nullable;
import android.support.v4.view.ViewCompat;
import android.util.AttributeSet;
import android.widget.ImageView;

import org.chromium.ui.R;

/**
 * A custom {@link ImageView} that is able to render bitmaps and colors with rounded off corners.
 * The corner radii should be set through attributes. E.g.
 *
 *   <org.chromium.ui.widget.RoundedCornerImageView
 *      app:cornerRadiusTopStart="8dp"
 *      app:cornerRadiusTopEnd="8dp"
 *      app:cornerRadiusBottomStart="8dp"
 *      app:cornerRadiusBottomEnd="8dp" />
 *
 * Note : This does not properly handle padding. Padding will not be taken into account when rounded
 * corners are used.
 */
public class RoundedCornerImageView extends ImageView {
    private Shape mRoundedRectangle;
    private BitmapShader mShader;
    private Paint mPaint;

    private Paint mFillPaint;

    // Whether or not to apply the shader, if we have one. This might be set to false if the image
    // is smaller than the view and does not need to have the corners rounded.
    private boolean mApplyShader;

    public RoundedCornerImageView(Context context) {
        this(context, null, 0);
    }

    public RoundedCornerImageView(Context context, AttributeSet attrs) {
        this(context, attrs, 0);
    }

    public RoundedCornerImageView(Context context, AttributeSet attrs, int defStyle) {
        super(context, attrs, defStyle);
        if (attrs != null) {
            TypedArray a = getContext().obtainStyledAttributes(
                    attrs, R.styleable.RoundedCornerImageView, 0, 0);
            int cornerRadiusTopStart = a.getDimensionPixelSize(
                    R.styleable.RoundedCornerImageView_cornerRadiusTopStart, 0);
            int cornerRadiusTopEnd = a.getDimensionPixelSize(
                    R.styleable.RoundedCornerImageView_cornerRadiusTopEnd, 0);
            int cornerRadiusBottomStart = a.getDimensionPixelSize(
                    R.styleable.RoundedCornerImageView_cornerRadiusBottomStart, 0);
            int cornerRadiusBottomEnd = a.getDimensionPixelSize(
                    R.styleable.RoundedCornerImageView_cornerRadiusBottomEnd, 0);
            if (a.hasValue(R.styleable.RoundedCornerImageView_roundedfillColor)) {
                mFillPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
                mFillPaint.setColor(a.getColor(
                        R.styleable.RoundedCornerImageView_roundedfillColor, Color.WHITE));
            }
            a.recycle();

            setRoundedCorners(cornerRadiusTopStart, cornerRadiusTopEnd, cornerRadiusBottomStart,
                    cornerRadiusBottomEnd);
        }
    }

    /**
     * Updates the rounded corners, using the radius set in the layout.
     */
    private void setRoundedCorners(int cornerRadiusTopStart, int cornerRadiusTopEnd,
            int cornerRadiusBottomStart, int cornerRadiusBottomEnd) {
        float[] radii;
        if (ViewCompat.getLayoutDirection(this) == ViewCompat.LAYOUT_DIRECTION_LTR) {
            radii = new float[] {cornerRadiusTopStart, cornerRadiusTopStart, cornerRadiusTopEnd,
                    cornerRadiusTopEnd, cornerRadiusBottomEnd, cornerRadiusBottomEnd,
                    cornerRadiusBottomStart, cornerRadiusBottomStart};
        } else {
            radii = new float[] {cornerRadiusTopEnd, cornerRadiusTopEnd, cornerRadiusTopStart,
                    cornerRadiusTopStart, cornerRadiusBottomStart, cornerRadiusBottomStart,
                    cornerRadiusBottomEnd, cornerRadiusBottomEnd};
        }

        mRoundedRectangle = new RoundRectShape(radii, null, null);
        mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    }

    @Override
    public void setImageDrawable(@Nullable Drawable drawable) {
        super.setImageDrawable(drawable);

        // Reset shaders.  We will need to recalculate them.
        mShader = null;
        mApplyShader = false;

        maybeCreateShader();

        updateApplyShader();
    }

    protected void maybeCreateShader() {
        // Only create the shader if we have a rectangle to use as a mask.
        Drawable drawable = getDrawable();
        Bitmap bitmap = (drawable instanceof BitmapDrawable)
                ? ((BitmapDrawable) drawable).getBitmap()
                : null;
        if (mRoundedRectangle != null && bitmap != null) {
            mShader = new BitmapShader(bitmap, Shader.TileMode.CLAMP, Shader.TileMode.CLAMP);
        }
    }

    @Override
    protected boolean setFrame(int l, int t, int r, int b) {
        boolean changed = super.setFrame(l, t, r, b);
        updateApplyShader();
        return changed;
    }

    @Override
    public void setScaleType(ScaleType scaleType) {
        super.setScaleType(scaleType);
        updateApplyShader();
    }

    /**
     * Updates the flag to tell whether or not to apply the shader that produces the rounded
     * corners. We should not apply the shader if the final image is smaller than the view, because
     * it will try to tile the image, which is not desirable. This should be called when the image
     * is changed, or the view bounds change.
     */
    private void updateApplyShader() {
        Drawable drawable = getDrawable();
        if ((drawable == null) || !(drawable instanceof BitmapDrawable) || (mShader == null)
                || (mPaint == null)) {
            // In this state we wouldn't use the shader anyway.
            mApplyShader = false;
            return;
        }

        // Default to using the shader.
        mApplyShader = true;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        Drawable drawable = getDrawable();
        Shape localRoundedRect = mRoundedRectangle;
        Paint localPaint = mPaint;

        boolean drawFill = mFillPaint != null && localRoundedRect != null
                && !(drawable instanceof ColorDrawable);
        boolean drawContent = drawable != null && localPaint != null && localRoundedRect != null
                && isSupportedDrawable(drawable);

        if (drawFill || drawContent) localRoundedRect.resize(getWidth(), getHeight());

        if (drawFill) localRoundedRect.draw(canvas, mFillPaint);

        if (!drawContent) {
            super.onDraw(canvas);
            return;
        }

        if (drawable instanceof ColorDrawable) {
            ColorDrawable colorDrawable = (ColorDrawable) drawable;
            localPaint.setColor(colorDrawable.getColor());
        }

        if (mShader != null && mApplyShader) {
            mShader.setLocalMatrix(getImageMatrix());
            localPaint.setShader(mShader);
        }

        localRoundedRect.draw(canvas, localPaint);
    }

    private boolean isSupportedDrawable(Drawable drawable) {
        return (drawable instanceof ColorDrawable) || (drawable instanceof BitmapDrawable);
    }
}
