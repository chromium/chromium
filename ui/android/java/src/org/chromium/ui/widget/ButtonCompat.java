// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.Color;
import android.util.AttributeSet;
import android.view.ContextThemeWrapper;

import androidx.annotation.ColorRes;
import androidx.annotation.StyleRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.appcompat.widget.AppCompatButton;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.R;
import org.chromium.ui.widget.RippleBackgroundHelper.BorderType;

/**
 * A Material-styled button with a customizable background color. On L devices, this is a true
 * Material button. On earlier devices, the button is similar but lacks ripples and a shadow.
 *
 * <p>Create a button in Java:
 *
 * <p>new ButtonCompat(context, R.style.TextButtonThemeOverlay);
 *
 * <p>Create a button in XML:
 *
 * <p><org.chromium.ui.widget.ButtonCompat android:layout_width="wrap_content"
 * android:layout_height="wrap_content" android:text="Click me" style="@style/TextButton" />
 *
 * <p>Note: To ensure the button's shadow is fully visible, you may need to set
 * android:clipToPadding="false" on the button's parent view.
 *
 * <p>See {@link R.styleable#ButtonCompat ButtonCompat Attributes}.
 */
@NullMarked
public class ButtonCompat extends AppCompatButton {
    private final RippleBackgroundHelper mRippleBackgroundHelper;

    /**
     * Constructor for programmatically creating a {@link ButtonCompat}.
     * @param context The {@link Context} for this class.
     * @param themeOverlay The style resource id that sets android:buttonStyle to specify the button
     *                     style.
     */
    public ButtonCompat(Context context, @StyleRes int themeOverlay) {
        this(context, null, themeOverlay);
    }

    /** Constructor for inflating from XMLs. */
    public ButtonCompat(Context context, AttributeSet attrs) {
        this(context, attrs, R.style.FilledButtonThemeOverlay);
    }

    private ButtonCompat(
            Context context, @Nullable AttributeSet attrs, @StyleRes int themeOverlay) {
        super(new ContextThemeWrapper(context, themeOverlay), attrs, android.R.attr.buttonStyle);

        TypedArray a =
                getContext()
                        .obtainStyledAttributes(
                                attrs, R.styleable.ButtonCompat, android.R.attr.buttonStyle, 0);
        int buttonColorId =
                a.getResourceId(
                        R.styleable.ButtonCompat_buttonColor, R.color.blue_when_enabled_list);

        int rippleColorId = a.getResourceId(R.styleable.ButtonCompat_rippleColor, -1);
        if (rippleColorId == -1) {
            // If we can't resolve rippleColor, e.g. we're provided an attr that's not available in
            // the theme, we'll use a fallback color based on the button color. A transparent color
            // means a text button, which should have a blue ripple while a filled button should
            // have a white ripple.
            boolean isBgTransparent = getContext().getColor(buttonColorId) == Color.TRANSPARENT;
            rippleColorId =
                    isBgTransparent
                            ? R.color.text_button_ripple_color_list_baseline
                            : R.color.filled_button_ripple_color;
        }

        int borderColorId =
                a.getResourceId(R.styleable.ButtonCompat_borderColor, android.R.color.transparent);
        int borderWidthId =
                a.getResourceId(
                        R.styleable.ButtonCompat_buttonBorderWidth,
                        R.dimen.default_ripple_background_border_size);
        int verticalInset =
                a.getDimensionPixelSize(
                        R.styleable.ButtonCompat_verticalInset,
                        getResources().getDimensionPixelSize(R.dimen.button_bg_vertical_inset));

        // Border style attribute
        @BorderType
        int borderStyle = a.getInt(R.styleable.ButtonCompat_buttonBorderStyle, BorderType.SOLID);

        final int defaultRadius =
                getResources().getDimensionPixelSize(R.dimen.button_compat_corner_radius);
        final int topStartRippleRadius =
                a.getDimensionPixelSize(
                        R.styleable.ButtonCompat_rippleCornerRadiusTopStart, defaultRadius);
        final int topEndRippleRadius =
                a.getDimensionPixelSize(
                        R.styleable.ButtonCompat_rippleCornerRadiusTopEnd, defaultRadius);
        final int bottomStartRippleRadius =
                a.getDimensionPixelSize(
                        R.styleable.ButtonCompat_rippleCornerRadiusBottomStart, defaultRadius);
        final int bottomEndRippleRadius =
                a.getDimensionPixelSize(
                        R.styleable.ButtonCompat_rippleCornerRadiusBottomEnd, defaultRadius);

        // If this attribute is not set, the text will keep the color set by android:textAppearance.
        // This would have been handled in #super().
        final @ColorRes int textColorRes =
                a.getResourceId(R.styleable.ButtonCompat_buttonTextColor, -1);

        if (textColorRes != -1) {
            setTextColor(AppCompatResources.getColorStateList(getContext(), textColorRes));
        }

        float[] radii;
        if (getLayoutDirection() == LAYOUT_DIRECTION_RTL) {
            radii =
                    new float[] {
                        topEndRippleRadius,
                        topEndRippleRadius,
                        topStartRippleRadius,
                        topStartRippleRadius,
                        bottomStartRippleRadius,
                        bottomStartRippleRadius,
                        bottomEndRippleRadius,
                        bottomEndRippleRadius
                    };
        } else {
            radii =
                    new float[] {
                        topStartRippleRadius,
                        topStartRippleRadius,
                        topEndRippleRadius,
                        topEndRippleRadius,
                        bottomEndRippleRadius,
                        bottomEndRippleRadius,
                        bottomStartRippleRadius,
                        bottomStartRippleRadius
                    };
        }

        a.recycle();
        mRippleBackgroundHelper =
                new RippleBackgroundHelper(
                        this,
                        buttonColorId,
                        rippleColorId,
                        radii,
                        borderColorId,
                        borderWidthId,
                        verticalInset);

        setBorderStyle(borderStyle);
    }

    /** Sets the background color of the button. */
    public void setButtonColor(ColorStateList buttonColorList) {
        mRippleBackgroundHelper.setBackgroundColor(buttonColorList);
    }

    /**
     * Sets the border style for the button.
     *
     * @param borderType The type of border (SOLID or DASHED).
     */
    public void setBorderStyle(@BorderType int borderType) {
        mRippleBackgroundHelper.setBorderStyle(borderType);
    }

    /**
     * Sets the border color for the button.
     *
     * @param borderColor The color that is drawn around the button with the current style.
     */
    public void setBorderColor(ColorStateList borderColor) {
        mRippleBackgroundHelper.setBorderColor(borderColor);
    }
}
