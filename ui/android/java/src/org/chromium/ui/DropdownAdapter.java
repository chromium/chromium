// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.LayerDrawable;
import android.text.TextUtils;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AbsListView.LayoutParams;
import android.widget.ArrayAdapter;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.view.MarginLayoutParamsCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.theme.FillInContextThemeWrapper;
import org.chromium.ui.util.AttrUtils;
import org.chromium.ui.widget.HoverHighlightViewListener;

import java.util.List;

/** Dropdown item adapter for DropdownPopupWindow. */
@NullMarked
public class DropdownAdapter extends ArrayAdapter<DropdownItem> {
    private final Context mContext;
    private final boolean mAreAllItemsEnabled;
    private final int mItemHeight;
    private final double mFontSize;
    private final HoverHighlightViewListener mHoverListener = new HoverHighlightViewListener();

    /**
     * Creates an {@code ArrayAdapter} with specified parameters.
     *
     * @param context Application context.
     * @param items List of labels and icons to display.
     * @param itemHeight The height of each item in the dropdown in pixels.
     * @param fontSize The font size of the label text in pixels.
     */
    public DropdownAdapter(
            Context context, List<? extends DropdownItem> items, int itemHeight, double fontSize) {
        super(
                new FillInContextThemeWrapper(
                        context, R.style.ThemeOverlay_UI_AdaptiveDensityDefaults),
                R.layout.dropdown_item);
        mContext = getContext();
        addAll(items);
        mAreAllItemsEnabled = checkAreAllItemsEnabled();
        mItemHeight = itemHeight;
        mFontSize = fontSize;
    }

    private boolean checkAreAllItemsEnabled() {
        for (int i = 0; i < getCount(); i++) {
            DropdownItem item = assumeNonNull(getItem(i));
            if (item.isEnabled() && !item.isGroupHeader()) {
                return false;
            }
        }
        return true;
    }

    @Override
    // Suppress warning because the font size and color of the dropdown items are dynamic
    // (determined by the HTML select element via Blink) and cannot be predefined in styles.xml.
    @SuppressWarnings("checkstyle:SetTextColorAndSetTextSizeCheck")
    public View getView(int position, @Nullable View convertView, ViewGroup parent) {
        View layout;
        if (convertView != null) {
            layout = convertView;
        } else {
            LayoutInflater inflater =
                    (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
            layout = inflater.inflate(R.layout.dropdown_item, null);
            layout.setOnHoverListener(mHoverListener);

            // DropdownDividerDrawable to draw the divider, listChoiceBackgroundIndicator to draw
            // ripple, hover, and focus effects.
            DropdownDividerDrawable divider =
                    new DropdownDividerDrawable(/* backgroundColor= */ null);
            LayerDrawable background = new LayerDrawable(new Drawable[] {divider});
            TypedValue typedValue = new TypedValue();
            if (mContext.getTheme()
                            .resolveAttribute(
                                    R.attr.listChoiceBackgroundIndicator, typedValue, true)
                    && typedValue.resourceId != 0) {
                background.addLayer(
                        AppCompatResources.getDrawable(mContext, typedValue.resourceId));
            }
            layout.setBackground(background);
        }
        LayerDrawable layers = (LayerDrawable) layout.getBackground();
        DropdownDividerDrawable divider = (DropdownDividerDrawable) layers.getDrawable(0);
        int height =
                Math.max(
                        mItemHeight,
                        AttrUtils.getDimensionPixelSize(mContext, R.attr.minInteractTargetSize));

        if (position == 0) {
            divider.setDividerColor(Color.TRANSPARENT);
        } else {
            int dividerHeight =
                    mContext.getResources()
                            .getDimensionPixelSize(R.dimen.dropdown_item_divider_height);
            height += dividerHeight;
            divider.setHeight(dividerHeight);
            divider.setDividerColor(mContext.getColor(R.color.dropdown_divider_color));
        }

        DropdownItem item = assumeNonNull(getItem(position));

        // Note: trying to set the height of the root LinearLayout breaks accessibility,
        // so we have to adjust the height of this LinearLayout that wraps the TextViews instead.
        // If you need to modify this layout, don't forget to test it with TalkBack and make sure
        // it doesn't regress.
        // http://crbug.com/429364
        LinearLayout wrapper = layout.findViewById(R.id.dropdown_label_wrapper);
        wrapper.setOrientation(LinearLayout.VERTICAL);
        wrapper.setLayoutParams(new LinearLayout.LayoutParams(0, height, 1));

        // Layout of the main label view.
        TextView labelView = layout.findViewById(R.id.dropdown_label);
        labelView.setText(item.getLabel());
        labelView.setSingleLine(true);

        labelView.setEnabled(item.isEnabled());
        if (item.isGroupHeader()) {
            labelView.setTypeface(null, Typeface.BOLD);
        } else {
            labelView.setTypeface(null, Typeface.NORMAL);
        }

        labelView.setTextColor(mContext.getColor(item.getLabelFontColorResId()));
        labelView.setTextSize(TypedValue.COMPLEX_UNIT_PX, (float) mFontSize);

        // Layout of the sublabel view, which has a smaller font and usually sits below the main
        // label.
        TextView sublabelView = layout.findViewById(R.id.dropdown_sublabel);
        CharSequence sublabel = item.getSublabel();
        if (TextUtils.isEmpty(sublabel)) {
            sublabelView.setVisibility(View.GONE);
        } else {
            sublabelView.setText(sublabel);
            sublabelView.setTextSize(
                    TypedValue.COMPLEX_UNIT_PX,
                    mContext.getResources().getDimension(R.dimen.text_size_small));
            sublabelView.setVisibility(View.VISIBLE);
        }

        ImageView iconView = layout.findViewById(R.id.end_dropdown_icon);
        if (item.getIconId() == DropdownItem.NO_ICON) {
            iconView.setVisibility(View.GONE);
        } else {
            ViewGroup.MarginLayoutParams iconLayoutParams =
                    (ViewGroup.MarginLayoutParams) iconView.getLayoutParams();
            iconLayoutParams.width = LayoutParams.WRAP_CONTENT;
            iconLayoutParams.height = LayoutParams.WRAP_CONTENT;
            int iconMargin =
                    mContext.getResources().getDimensionPixelSize(R.dimen.dropdown_icon_margin);
            MarginLayoutParamsCompat.setMarginStart(iconLayoutParams, iconMargin);
            MarginLayoutParamsCompat.setMarginEnd(iconLayoutParams, iconMargin);
            iconView.setLayoutParams(iconLayoutParams);
            iconView.setImageDrawable(AppCompatResources.getDrawable(mContext, item.getIconId()));
            iconView.setVisibility(View.VISIBLE);
        }

        return layout;
    }

    @Override
    public boolean areAllItemsEnabled() {
        return mAreAllItemsEnabled;
    }

    @Override
    public boolean isEnabled(int position) {
        if (position < 0 || position >= getCount()) return false;
        DropdownItem item = assumeNonNull(getItem(position));
        return item.isEnabled() && !item.isGroupHeader();
    }
}
