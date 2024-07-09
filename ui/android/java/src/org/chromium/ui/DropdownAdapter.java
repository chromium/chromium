// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
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
import androidx.core.view.ViewCompat;

import java.util.List;
import java.util.Set;

/** Dropdown item adapter for DropdownPopupWindow. */
public class DropdownAdapter extends ArrayAdapter<DropdownItem> {
    private final Context mContext;
    private final Set<Integer> mSeparators;
    private final boolean mAreAllItemsEnabled;
    private final int mLabelMargin;

    /**
     * Creates an {@code ArrayAdapter} with specified parameters.
     * @param context Application context.
     * @param items List of labels and icons to display.
     * @param separators Set of positions that separate {@code items}.
     */
    public DropdownAdapter(
            Context context, List<? extends DropdownItem> items, Set<Integer> separators) {
        super(context, R.layout.dropdown_item);
        mContext = context;
        addAll(items);
        mSeparators = separators;
        mAreAllItemsEnabled = checkAreAllItemsEnabled();
        mLabelMargin =
                context.getResources().getDimensionPixelSize(R.dimen.dropdown_item_label_margin);
    }

    private boolean checkAreAllItemsEnabled() {
        for (int i = 0; i < getCount(); i++) {
            DropdownItem item = getItem(i);
            if (item.isEnabled() && !item.isGroupHeader()) {
                return false;
            }
        }
        return true;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        View layout = convertView;
        if (convertView == null) {
            LayoutInflater inflater =
                    (LayoutInflater) mContext.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
            layout = inflater.inflate(R.layout.dropdown_item, null);
            layout.setBackground(new DropdownDividerDrawable(/* backgroundColor= */ null));
        }
        DropdownDividerDrawable divider = (DropdownDividerDrawable) layout.getBackground();
        int height = mContext.getResources().getDimensionPixelSize(R.dimen.dropdown_item_height);

        if (position == 0) {
            divider.setDividerColor(Color.TRANSPARENT);
        } else {
            int dividerHeight =
                    mContext.getResources()
                            .getDimensionPixelSize(R.dimen.dropdown_item_divider_height);
            height += dividerHeight;
            divider.setHeight(dividerHeight);
            int dividerColor;
            if (mSeparators != null && mSeparators.contains(position)) {
                dividerColor = mContext.getColor(R.color.dropdown_dark_divider_color);
            } else {
                dividerColor = mContext.getColor(R.color.dropdown_divider_color);
            }
            divider.setDividerColor(dividerColor);
        }

        DropdownItem item = getItem(position);

        // Note: trying to set the height of the root LinearLayout breaks accessibility,
        // so we have to adjust the height of this LinearLayout that wraps the TextViews instead.
        // If you need to modify this layout, don't forget to test it with TalkBack and make sure
        // it doesn't regress.
        // http://crbug.com/429364
        LinearLayout wrapper = (LinearLayout) layout.findViewById(R.id.dropdown_label_wrapper);
        if (item.isMultilineLabel()) height = LayoutParams.WRAP_CONTENT;
        wrapper.setOrientation(LinearLayout.VERTICAL);
        wrapper.setLayoutParams(new LinearLayout.LayoutParams(0, height, 1));

        // Layout of the main label view.
        TextView labelView = (TextView) layout.findViewById(R.id.dropdown_label);
        labelView.setText(item.getLabel());
        labelView.setSingleLine(!item.isMultilineLabel());
        if (item.isMultilineLabel()) {
            // If there is a multiline label, we add extra padding at the top and bottom because
            // WRAP_CONTENT, defined above for multiline labels, leaves none.
            int existingStart = ViewCompat.getPaddingStart(labelView);
            int existingEnd = ViewCompat.getPaddingEnd(labelView);
            labelView.setPaddingRelative(existingStart, mLabelMargin, existingEnd, mLabelMargin);
        }

        labelView.setEnabled(item.isEnabled());
        if (item.isGroupHeader() || item.isBoldLabel()) {
            labelView.setTypeface(null, Typeface.BOLD);
        } else {
            labelView.setTypeface(null, Typeface.NORMAL);
        }

        labelView.setTextColor(mContext.getColor(item.getLabelFontColorResId()));
        labelView.setTextSize(
                TypedValue.COMPLEX_UNIT_PX,
                mContext.getResources().getDimension(R.dimen.text_size_large));

        // Layout of the sublabel view, which has a smaller font and usually sits below the main
        // label.
        TextView sublabelView = (TextView) layout.findViewById(R.id.dropdown_sublabel);
        CharSequence sublabel = item.getSublabel();
        if (TextUtils.isEmpty(sublabel)) {
            sublabelView.setVisibility(View.GONE);
        } else {
            sublabelView.setText(sublabel);
            sublabelView.setTextSize(
                    TypedValue.COMPLEX_UNIT_PX,
                    mContext.getResources().getDimension(item.getSublabelFontSizeResId()));
            sublabelView.setVisibility(View.VISIBLE);
        }

        ImageView iconViewStart = (ImageView) layout.findViewById(R.id.start_dropdown_icon);
        ImageView iconViewEnd = (ImageView) layout.findViewById(R.id.end_dropdown_icon);
        if (item.isIconAtStart()) {
            iconViewEnd.setVisibility(View.GONE);
        } else {
            iconViewStart.setVisibility(View.GONE);
        }

        ImageView iconView = item.isIconAtStart() ? iconViewStart : iconViewEnd;
        if (item.getIconId() == DropdownItem.NO_ICON) {
            iconView.setVisibility(View.GONE);
        } else {
            int iconSizeResId = item.getIconSizeResId();
            int iconSize =
                    iconSizeResId == 0
                            ? LayoutParams.WRAP_CONTENT
                            : mContext.getResources().getDimensionPixelSize(iconSizeResId);
            ViewGroup.MarginLayoutParams iconLayoutParams =
                    (ViewGroup.MarginLayoutParams) iconView.getLayoutParams();
            iconLayoutParams.width = iconSize;
            iconLayoutParams.height = iconSize;
            int iconMargin =
                    mContext.getResources().getDimensionPixelSize(item.getIconMarginResId());
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
        DropdownItem item = getItem(position);
        return item.isEnabled() && !item.isGroupHeader();
    }
}
