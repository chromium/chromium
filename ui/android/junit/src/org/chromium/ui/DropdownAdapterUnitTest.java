// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.LayerDrawable;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.util.AttrUtils;

import java.util.List;

/** Unit tests for {@link DropdownAdapter}. */
@RunWith(BaseRobolectricTestRunner.class)
public class DropdownAdapterUnitTest {
    private static final int ITEM_HEIGHT_PX = 40;
    private static final double FONT_SIZE_PX = 14.0;

    private Context mContext;
    private FrameLayout mParent;

    private static class TestDropdownItem extends DropdownItemBase {
        private final String mLabel;
        private final @Nullable String mSublabel;
        private final boolean mEnabled;
        private final boolean mIsGroupHeader;
        private final int mIconId;

        TestDropdownItem(String label, boolean enabled, boolean isGroupHeader) {
            this(label, /* sublabel= */ null, enabled, isGroupHeader, DropdownItem.NO_ICON);
        }

        TestDropdownItem(
                String label,
                @Nullable String sublabel,
                boolean enabled,
                boolean isGroupHeader,
                int iconId) {
            mLabel = label;
            mSublabel = sublabel;
            mEnabled = enabled;
            mIsGroupHeader = isGroupHeader;
            mIconId = iconId;
        }

        @Override
        public String getLabel() {
            return mLabel;
        }

        @Override
        public @Nullable String getSublabel() {
            return mSublabel;
        }

        @Override
        public boolean isEnabled() {
            return mEnabled;
        }

        @Override
        public boolean isGroupHeader() {
            return mIsGroupHeader;
        }

        @Override
        public int getIconId() {
            return mIconId;
        }
    }

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mParent = new FrameLayout(mContext);
    }

    @Test
    public void isEnabled_returnsTrueOnlyForEnabledNonGroupItems() {
        DropdownAdapter adapter =
                createAdapter(
                        groupHeader("Group"), enabledOption("Enabled"), disabledOption("Disabled"));

        assertFalse(adapter.isEnabled(-1));
        assertFalse(adapter.isEnabled(0));
        assertTrue(adapter.isEnabled(1));
        assertFalse(adapter.isEnabled(2));
        assertFalse(adapter.isEnabled(3));
    }

    @Test
    public void getView_bindsLabelSublabelGroupStyleAndIcon() {
        DropdownAdapter adapter =
                createAdapter(
                        groupHeader("Group"),
                        new TestDropdownItem(
                                "With details",
                                "Sublabel text",
                                /* enabled= */ true,
                                /* isGroupHeader= */ false,
                                R.drawable.ic_delete_fill_24dp),
                        disabledOption("Disabled"));

        View groupView = adapter.getView(0, /* convertView= */ null, mParent);
        TextView groupLabel = groupView.findViewById(R.id.dropdown_label);
        TextView groupSublabel = groupView.findViewById(R.id.dropdown_sublabel);
        ImageView groupIcon = groupView.findViewById(R.id.end_dropdown_icon);
        assertEquals("Group", groupLabel.getText().toString());
        assertEquals(Typeface.defaultFromStyle(Typeface.BOLD), groupLabel.getTypeface());
        assertTrue(groupLabel.isEnabled());
        assertEquals(View.GONE, groupSublabel.getVisibility());
        assertEquals(View.GONE, groupIcon.getVisibility());

        View detailView = adapter.getView(1, /* convertView= */ null, mParent);
        TextView detailLabel = detailView.findViewById(R.id.dropdown_label);
        TextView detailSublabel = detailView.findViewById(R.id.dropdown_sublabel);
        ImageView detailIcon = detailView.findViewById(R.id.end_dropdown_icon);
        assertEquals("With details", detailLabel.getText().toString());
        assertNull(detailLabel.getTypeface());
        assertTrue(detailLabel.isEnabled());
        assertEquals(View.VISIBLE, detailSublabel.getVisibility());
        assertEquals("Sublabel text", detailSublabel.getText().toString());
        assertEquals(View.VISIBLE, detailIcon.getVisibility());
        assertNotNull(detailIcon.getDrawable());

        View disabledView = adapter.getView(2, detailView, mParent);
        TextView disabledLabel = disabledView.findViewById(R.id.dropdown_label);
        TextView disabledSublabel = disabledView.findViewById(R.id.dropdown_sublabel);
        ImageView disabledIcon = disabledView.findViewById(R.id.end_dropdown_icon);
        assertEquals("Disabled", disabledLabel.getText().toString());
        assertFalse(disabledLabel.isEnabled());
        assertEquals(View.GONE, disabledSublabel.getVisibility());
        assertEquals(View.GONE, disabledIcon.getVisibility());
    }

    @Test
    public void getView_showsDividersForEveryNonFirstItemWhenUngrouped() {
        DropdownAdapter adapter =
                createAdapter(enabledOption("One"), enabledOption("Two"), enabledOption("Three"));

        int baseHeight = expectedBaseHeight(adapter);
        int dividerHeight = expectedDividerHeight(adapter);
        int dividerColor = adapter.getContext().getColor(R.color.dropdown_divider_color);

        assertDividerState(
                adapter.getView(0, /* convertView= */ null, mParent),
                Color.TRANSPARENT,
                baseHeight);
        View secondItemView = adapter.getView(1, /* convertView= */ null, mParent);
        assertDividerState(secondItemView, dividerColor, baseHeight + dividerHeight);
        assertDividerState(
                adapter.getView(2, /* convertView= */ null, mParent),
                dividerColor,
                baseHeight + dividerHeight);
        assertDividerState(
                adapter.getView(0, secondItemView, mParent), Color.TRANSPARENT, baseHeight);
    }

    @Test
    public void getView_dimsDisabledItemLabelColor() {
        DropdownAdapter adapter =
                createAdapter(
                        groupHeader("Group"), enabledOption("Enabled"), disabledOption("Disabled"));

        ColorStateList colorStateList =
                adapter.getContext().getColorStateList(R.color.default_text_color_list_baseline);
        int expectedEnabledColor =
                colorStateList.getColorForState(
                        new int[] {android.R.attr.state_enabled}, colorStateList.getDefaultColor());
        int expectedDisabledColor =
                colorStateList.getColorForState(
                        new int[] {-android.R.attr.state_enabled},
                        colorStateList.getDefaultColor());
        assertNotEquals(expectedEnabledColor, expectedDisabledColor);

        View groupView = adapter.getView(0, /* convertView= */ null, mParent);
        TextView groupLabel = groupView.findViewById(R.id.dropdown_label);
        assertEquals(expectedEnabledColor, groupLabel.getCurrentTextColor());

        View enabledView = adapter.getView(1, /* convertView= */ null, mParent);
        TextView enabledLabel = enabledView.findViewById(R.id.dropdown_label);
        assertEquals(expectedEnabledColor, enabledLabel.getCurrentTextColor());

        View disabledView = adapter.getView(2, /* convertView= */ null, mParent);
        TextView disabledLabel = disabledView.findViewById(R.id.dropdown_label);
        assertEquals(expectedDisabledColor, disabledLabel.getCurrentTextColor());
    }

    private static int expectedBaseHeight(DropdownAdapter adapter) {
        return Math.max(
                ITEM_HEIGHT_PX,
                AttrUtils.getDimensionPixelSize(
                        adapter.getContext(), R.attr.minInteractTargetSize));
    }

    private static int expectedDividerHeight(DropdownAdapter adapter) {
        return adapter.getContext()
                .getResources()
                .getDimensionPixelSize(R.dimen.dropdown_item_divider_height);
    }

    private static void assertDividerState(View rowView, int expectedColor, int expectedHeight) {
        LinearLayout wrapper = rowView.findViewById(R.id.dropdown_label_wrapper);
        assertEquals(expectedHeight, wrapper.getLayoutParams().height);

        LayerDrawable background = (LayerDrawable) rowView.getBackground();
        DropdownDividerDrawable divider = (DropdownDividerDrawable) background.getDrawable(0);
        Bitmap bitmap = Bitmap.createBitmap(4, 4, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        divider.setBounds(0, 0, 4, 4);
        divider.draw(canvas);
        assertEquals(expectedColor, bitmap.getPixel(0, 0));
    }

    private static TestDropdownItem enabledOption(String label) {
        return new TestDropdownItem(label, /* enabled= */ true, /* isGroupHeader= */ false);
    }

    private static TestDropdownItem disabledOption(String label) {
        return new TestDropdownItem(label, /* enabled= */ false, /* isGroupHeader= */ false);
    }

    private static TestDropdownItem groupHeader(String label) {
        return new TestDropdownItem(label, /* enabled= */ true, /* isGroupHeader= */ true);
    }

    private DropdownAdapter createAdapter(DropdownItem... items) {
        return new DropdownAdapter(mContext, List.of(items), ITEM_HEIGHT_PX, FONT_SIZE_PX);
    }
}
