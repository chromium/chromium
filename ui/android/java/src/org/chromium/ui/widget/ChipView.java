// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.ui.widget;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.support.v4.view.ViewCompat;
import android.util.AttributeSet;
import android.view.ContextThemeWrapper;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.IdRes;
import androidx.annotation.Px;
import androidx.annotation.StyleRes;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.ui.R;

/** The view responsible for displaying a material chip. */
public class ChipView extends LinearLayout {
    /** An id to use for {@link #setIcon(int, boolean)} when there is no icon on the chip. */
    public static final int INVALID_ICON_ID = -1;

    private final RippleBackgroundHelper mRippleBackgroundHelper;
    private final TextView mPrimaryText;
    private final ChromeImageView mIcon;
    private final @IdRes int mSecondaryTextAppearanceId;

    private TextView mSecondaryText;

    /**
     * Constructor for inflating from XML.
     */
    public ChipView(Context context, @StyleRes int chipStyle) {
        this(context, null, chipStyle);
    }

    /**
     * Constructor for inflating from XML.
     */
    public ChipView(Context context, AttributeSet attrs) {
        this(context, attrs, R.style.SuggestionChipThemeOverlay);
    }

    private ChipView(Context context, AttributeSet attrs, @StyleRes int themeOverlay) {
        super(new ContextThemeWrapper(context, themeOverlay), attrs, R.attr.chipStyle);

        final @Px int leadingElementPadding =
                getResources().getDimensionPixelSize(R.dimen.chip_element_leading_padding);
        final @Px int endPadding = getResources().getDimensionPixelSize(R.dimen.chip_end_padding);

        TypedArray a = getContext().obtainStyledAttributes(
                attrs, R.styleable.ChipView, R.attr.chipStyle, 0);
        int chipColorId =
                a.getResourceId(R.styleable.ChipView_chipColor, R.color.chip_background_color);
        int rippleColorId =
                a.getResourceId(R.styleable.ChipView_rippleColor, R.color.chip_ripple_color);
        int cornerRadius = a.getDimensionPixelSize(R.styleable.ChipView_cornerRadius,
                getContext().getResources().getDimensionPixelSize(R.dimen.chip_corner_radius));
        int iconWidth = a.getDimensionPixelSize(R.styleable.ChipView_iconWidth,
                getResources().getDimensionPixelSize(R.dimen.chip_icon_size));
        int iconHeight = a.getDimensionPixelSize(R.styleable.ChipView_iconHeight,
                getResources().getDimensionPixelSize(R.dimen.chip_icon_size));
        int primaryTextAppearance = a.getResourceId(
                R.styleable.ChipView_primaryTextAppearance, R.style.TextAppearance_ChipText);
        mSecondaryTextAppearanceId = a.getResourceId(
                R.styleable.ChipView_secondaryTextAppearance, R.style.TextAppearance_ChipText);
        int verticalInset = a.getDimensionPixelSize(R.styleable.ChipView_verticalInset,
                getResources().getDimensionPixelSize(R.dimen.chip_bg_vertical_inset));
        a.recycle();

        mIcon = new ChromeImageView(getContext());
        mIcon.setLayoutParams(new LayoutParams(iconWidth, iconHeight));
        addView(mIcon);

        // Setting this enforces 16dp padding at the end and 8dp at the start. For text, the start
        // padding needs to be 16dp which is why a ChipTextView contributes the remaining 8dp.
        ViewCompat.setPaddingRelative(this, leadingElementPadding, 0, endPadding, 0);

        mPrimaryText = new TextView(new ContextThemeWrapper(getContext(), R.style.ChipTextView));
        ApiCompatibilityUtils.setTextAppearance(mPrimaryText, primaryTextAppearance);
        addView(mPrimaryText);

        // Reset icon and background:
        mRippleBackgroundHelper = new RippleBackgroundHelper(this, chipColorId, rippleColorId,
                cornerRadius, R.color.chip_stroke_color, R.dimen.chip_border_width, verticalInset);
        setIcon(INVALID_ICON_ID, false);
    }

    @Override
    protected void drawableStateChanged() {
        super.drawableStateChanged();
        if (mRippleBackgroundHelper != null) {
            mRippleBackgroundHelper.onDrawableStateChanged();
        }
    }

    /**
     * Unlike setSelected, setEnabled doesn't properly propagate the new state to its subcomponents.
     * Enforce this so ColorStateLists used for the text appearance apply as intended.
     * @param enabled The new enabled state for the chip view and the TextViews owned by it.
     */
    @Override
    public void setEnabled(boolean enabled) {
        super.setEnabled(enabled);
        getPrimaryTextView().setEnabled(enabled);
        if (mSecondaryText != null) mSecondaryText.setEnabled(enabled);
    }

    /**
     * Sets the icon at the start of the chip view.
     * @param icon The resource id pointing to the icon.
     */
    public void setIcon(@DrawableRes int icon, boolean tintWithTextColor) {
        if (icon == INVALID_ICON_ID) {
            mIcon.setVisibility(ViewGroup.GONE);
            return;
        }

        mIcon.setVisibility(ViewGroup.VISIBLE);
        mIcon.setImageResource(icon);
        setTint(tintWithTextColor);
    }

    /**
     * Sets the icon at the start of the chip view.
     * @param drawable Drawable to display.
     */
    public void setIcon(Drawable drawable, boolean tintWithTextColor) {
        mIcon.setVisibility(ViewGroup.VISIBLE);
        mIcon.setImageDrawable(drawable);
        setTint(tintWithTextColor);
    }

    /**
     * Returns the {@link TextView} that contains the label of the chip.
     * @return A {@link TextView}.
     */
    public TextView getPrimaryTextView() {
        return mPrimaryText;
    }

    /**
     * Returns the {@link TextView} that contains the secondary label of the chip. If it wasn't used
     * until now, this creates the view.
     * @return A {@link TextView}.
     */
    public TextView getSecondaryTextView() {
        if (mSecondaryText == null) {
            mSecondaryText =
                    new TextView(new ContextThemeWrapper(getContext(), R.style.ChipTextView));
            ApiCompatibilityUtils.setTextAppearance(mSecondaryText, mSecondaryTextAppearanceId);
            // Ensure that basic state changes are aligned with the ChipView. They update
            // automatically once the view is part of the hierarchy.
            mSecondaryText.setSelected(isSelected());
            mSecondaryText.setEnabled(isEnabled());
            addView(mSecondaryText);
        }
        return mSecondaryText;
    }

    /**
     * Sets the correct tinting on the Chip's image view.
     * @param tintWithTextColor If true then the image view will be tinted with the primary text
     *      color. If not, the tint will be cleared.
     */
    private void setTint(boolean tintWithTextColor) {
        if (mPrimaryText.getTextColors() != null && tintWithTextColor) {
            ApiCompatibilityUtils.setImageTintList(mIcon, mPrimaryText.getTextColors());
        } else {
            ApiCompatibilityUtils.setImageTintList(mIcon, null);
        }
    }
}
