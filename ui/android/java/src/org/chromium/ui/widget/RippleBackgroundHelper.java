// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.annotation.TargetApi;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.graphics.drawable.RippleDrawable;
import android.os.Build;
import android.support.annotation.ColorInt;
import android.support.annotation.ColorRes;
import android.support.annotation.Nullable;
import android.support.v4.graphics.ColorUtils;
import android.support.v4.graphics.drawable.DrawableCompat;
import android.support.v7.content.res.AppCompatResources;
import android.util.StateSet;
import android.view.View;

import org.chromium.ui.R;

/**
 * A helper class to create and maintain a background drawable with customized background color,
 * ripple color, and corner radius.
 */
class RippleBackgroundHelper {
    private static final int[] STATE_SET_PRESSED = {android.R.attr.state_pressed};
    private static final int[] STATE_SET_SELECTED = {android.R.attr.state_selected};
    private static final int[] STATE_SET_SELECTED_PRESSED = {
            android.R.attr.state_selected, android.R.attr.state_pressed};

    private final View mView;

    private @Nullable ColorStateList mBackgroundColorList;

    private GradientDrawable mBackgroundGradient;

    // Used for applying tint on pre-L versions.
    private Drawable mBackgroundDrawablePreL;
    private Drawable mRippleDrawablePreL;

    /**
     * @param view The {@link View} on which background will be applied.
     * @param backgroundColorResId The resource id of the background color.
     * @param rippleColorResId The resource id of the ripple color.
     */
    RippleBackgroundHelper(
            View view, @ColorRes int backgroundColorResId, @ColorRes int rippleColorResId) {
        mView = view;
        ColorStateList rippleColorList =
                AppCompatResources.getColorStateList(view.getContext(), rippleColorResId);

        int cornerRadius =
                view.getResources().getDimensionPixelSize(R.dimen.button_compat_corner_radius);
        ColorStateList backgroundColorList =
                AppCompatResources.getColorStateList(view.getContext(), backgroundColorResId);

        mBackgroundGradient = new GradientDrawable();
        mBackgroundGradient.setCornerRadius(cornerRadius);

        Drawable wrappedDrawable;
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            mBackgroundDrawablePreL = DrawableCompat.wrap(mBackgroundGradient);
            GradientDrawable rippleGradient = new GradientDrawable();
            rippleGradient.setCornerRadius(cornerRadius);
            mRippleDrawablePreL = DrawableCompat.wrap(rippleGradient);
            DrawableCompat.setTintList(mRippleDrawablePreL, rippleColorList);
            wrappedDrawable = new LayerDrawable(
                    new Drawable[] {mBackgroundDrawablePreL, mRippleDrawablePreL});
        } else {
            GradientDrawable mask = new GradientDrawable();
            mask.setCornerRadius(cornerRadius);
            mask.setColor(Color.WHITE);
            wrappedDrawable = new RippleDrawable(
                    convertToRippleDrawableColorList(rippleColorList), mBackgroundGradient, mask);
        }

        int paddingLeft = mView.getPaddingLeft();
        int paddingTop = mView.getPaddingTop();
        int paddingRight = mView.getPaddingRight();
        int paddingBottom = mView.getPaddingBottom();
        mView.setBackground(wrappedDrawable);
        setBackgroundColor(backgroundColorList);

        // On KitKat, setting the background on the view can cause padding reset. Save the padding
        // and re-apply after background is set.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            mView.setPadding(paddingLeft, paddingTop, paddingRight, paddingBottom);
        }
    }

    /**
     * @param color The {@link ColorStateList} to be set as the background color on the background
     *              drawable.
     */
    void setBackgroundColor(ColorStateList color) {
        if (color == mBackgroundColorList) return;

        mBackgroundColorList = color;
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            DrawableCompat.setTintList(mBackgroundDrawablePreL, color);
        } else {
            mBackgroundGradient.setColor(color);
        }
    }

    /**
     * Called from the view when drawable state is changed to update the state of the background
     * color and the ripple color for pre-L versions.
     */
    void onDrawableStateChanged() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) return;

        int[] state = mView.getDrawableState();
        mBackgroundDrawablePreL.setState(state);
        mRippleDrawablePreL.setState(state);
    }

    /**
     * @return The color under the specified states in the specified {@link ColorStateList}.
     */
    private static @ColorInt int getColorForState(ColorStateList colorStateList, int[] states) {
        return colorStateList.getColorForState(states, colorStateList.getDefaultColor());
    }

    /**
     * Adjusts the opacity of the ripple color since {@link RippleDrawable} uses about 50% opacity
     * of color for ripple effect.
     */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private @ColorInt static int doubleAlpha(@ColorInt int color) {
        int alpha = Math.min(Color.alpha(color) * 2, 255);
        return ColorUtils.setAlphaComponent(color, alpha);
    }

    /**
     * Converts the specified {@link ColorStateList} to one that can be applied to a
     * {@link RippleDrawable}.
     */
    @TargetApi(Build.VERSION_CODES.LOLLIPOP)
    private static ColorStateList convertToRippleDrawableColorList(ColorStateList colorStateList) {
        return new ColorStateList(new int[][] {STATE_SET_SELECTED, StateSet.NOTHING},
                new int[] {
                        doubleAlpha(getColorForState(colorStateList, STATE_SET_SELECTED_PRESSED)),
                        doubleAlpha(getColorForState(colorStateList, STATE_SET_PRESSED))});
    }
}
