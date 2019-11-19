// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.animation.AnimatorInflater;
import android.animation.StateListAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.os.Build;
import android.support.v7.widget.AppCompatButton;
import android.util.AttributeSet;
import android.view.ContextThemeWrapper;

import androidx.annotation.StyleRes;

import org.chromium.ui.R;

/**
 * A Material-styled button with a customizable background color. On L devices, this is a true
 * Material button. On earlier devices, the button is similar but lacks ripples and a shadow.
 *
 * Create a button in Java:
 *
 *   new ButtonCompat(context, R.style.TextButtonThemeOverlay);
 *
 * Create a button in XML:
 *
 *   <org.chromium.ui.widget.ButtonCompat
 *       android:layout_width="wrap_content"
 *       android:layout_height="wrap_content"
 *       android:text="Click me"
 *       style="@style/TextButton" />
 *
 * Note: To ensure the button's shadow is fully visible, you may need to set
 * android:clipToPadding="false" on the button's parent view.
 *
 * See {@link R.styleable#ButtonCompat ButtonCompat Attributes}.
 */
public class ButtonCompat extends AppCompatButton {
    private RippleBackgroundHelper mRippleBackgroundHelper;

    /**
     * Constructor for programmatically creating a {@link ButtonCompat}.
     * @param context The {@link Context} for this class.
     * @param themeOverlay The style resource id that sets android:buttonStyle to specify the button
     *                     style.
     */
    public ButtonCompat(Context context, @StyleRes int themeOverlay) {
        this(context, null, themeOverlay);
    }

    /**
     * Constructor for inflating from XMLs.
     */
    public ButtonCompat(Context context, AttributeSet attrs) {
        this(context, attrs, R.style.FilledButtonThemeOverlay);
    }

    private ButtonCompat(Context context, AttributeSet attrs, @StyleRes int themeOverlay) {
        super(new ContextThemeWrapper(context, themeOverlay), attrs, android.R.attr.buttonStyle);

        TypedArray a = getContext().obtainStyledAttributes(
                attrs, R.styleable.ButtonCompat, android.R.attr.buttonStyle, 0);
        int buttonColorId =
                a.getResourceId(R.styleable.ButtonCompat_buttonColor, R.color.blue_when_enabled);
        int rippleColorId = a.getResourceId(
                R.styleable.ButtonCompat_rippleColor, R.color.filled_button_ripple_color);
        boolean buttonRaised = a.getBoolean(R.styleable.ButtonCompat_buttonRaised, true);
        int verticalInset = a.getDimensionPixelSize(R.styleable.ButtonCompat_verticalInset,
                getResources().getDimensionPixelSize(R.dimen.button_bg_vertical_inset));
        a.recycle();

        mRippleBackgroundHelper = new RippleBackgroundHelper(this, buttonColorId, rippleColorId,
                getResources().getDimensionPixelSize(R.dimen.button_compat_corner_radius),
                verticalInset);
        setRaised(buttonRaised);
    }

    /**
     * Sets the background color of the button.
     */
    public void setButtonColor(ColorStateList buttonColorList) {
        mRippleBackgroundHelper.setBackgroundColor(buttonColorList);
    }

    /**
    * Sets whether the button is raised (has a shadow), or flat (has no shadow).
    * Note that this function (setStateListAnimator) can not be called more than once due to
    * incompatibilities in older android versions, crbug.com/608248.
    */
    private void setRaised(boolean raised) {
        // All buttons are flat on pre-L devices.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) return;

        if (raised) {
            // Use the StateListAnimator from the Widget.Material.Button style to animate the
            // elevation when the button is pressed.
            TypedArray a = getContext().obtainStyledAttributes(null,
                    new int[]{android.R.attr.stateListAnimator}, 0,
                    android.R.style.Widget_Material_Button);
            int stateListAnimatorId = a.getResourceId(0, 0);
            a.recycle();

            // stateListAnimatorId could be 0 on custom or future builds of Android, or when
            // using a framework like Xposed. Handle these cases gracefully by simply not using
            // a StateListAnimator.
            StateListAnimator stateListAnimator = null;
            if (stateListAnimatorId != 0) {
                stateListAnimator = AnimatorInflater.loadStateListAnimator(getContext(),
                        stateListAnimatorId);
            }
            setStateListAnimator(stateListAnimator);
        } else {
            setElevation(0f);
            setStateListAnimator(null);
        }
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();
        if (mRippleBackgroundHelper != null) {
            mRippleBackgroundHelper.onDrawableStateChanged();
        }
    }
}
