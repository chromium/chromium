// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.R;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link ListMenuItemViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ListMenuItemViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ViewGroup mListItemView;
    @Mock private LinearLayout mInnerLayout;
    @Mock private TextView mTextView;
    @Mock private ImageView mStartIcon;
    @Mock private ImageView mEndIcon;
    @Mock private ImageView mSubmenuArrow;
    @Mock private TextView mSubtitleView;
    @Mock private LayoutParams mLayoutParams;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        when(mListItemView.getContext()).thenReturn(mContext);
        when(mListItemView.getResources()).thenReturn(mContext.getResources());
        when(mListItemView.findViewById(R.id.menu_item_text)).thenReturn(mTextView);
        when(mListItemView.findViewById(R.id.menu_item_icon)).thenReturn(mStartIcon);
        when(mListItemView.findViewById(R.id.submenu_arrow)).thenReturn(mSubmenuArrow);
        when(mListItemView.findViewById(R.id.menu_item_end_icon)).thenReturn(mEndIcon);
        when(mListItemView.findViewById(R.id.menu_item_subtitle)).thenReturn(mSubtitleView);
        when(mStartIcon.getLayoutParams()).thenReturn(mLayoutParams);
        when(mStartIcon.getContext()).thenReturn(mContext);
        when(mEndIcon.getContext()).thenReturn(mContext);
        when(mSubmenuArrow.getContext()).thenReturn(mContext);

        // Required for ListMenuUtils.applyTintToAllIcons recursion to find icons.
        // Hierarchy from list_menu_item.xml:
        // mListItemView (LinearLayout)
        //   - mStartIcon (ImageView)
        //   - mInnerLayout (LinearLayout)
        //      - mTextView (TextView)
        //      - mSubtitleView (TextView)
        //   - mEndIcon (ImageView)
        //   - mSubmenuArrow (ImageView)
        when(mListItemView.getChildCount()).thenReturn(4);
        when(mListItemView.getChildAt(0)).thenReturn(mStartIcon);
        when(mListItemView.getChildAt(1)).thenReturn(mInnerLayout);
        when(mListItemView.getChildAt(2)).thenReturn(mEndIcon);
        when(mListItemView.getChildAt(3)).thenReturn(mSubmenuArrow);

        when(mInnerLayout.getChildCount()).thenReturn(2);
        when(mInnerLayout.getChildAt(0)).thenReturn(mTextView);
        when(mInnerLayout.getChildAt(1)).thenReturn(mSubtitleView);
    }

    @Test
    @SmallTest
    public void testSetTitle() {
        String title = "Test Title";
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TITLE, title)
                        .build();
        ListMenuItemViewBinder.binder(propertyModel, mListItemView, ListMenuItemProperties.TITLE);
        verify(mTextView).setText(title);
    }

    @Test
    @SmallTest
    public void testSetSubtitle() {
        String subtitle = "Test Subtitle";
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.SUBTITLE, subtitle)
                        .build();
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.SUBTITLE);
        verify(mSubtitleView).setText(subtitle);
        verify(mSubtitleView).setVisibility(View.VISIBLE);

        propertyModel.set(ListMenuItemProperties.SUBTITLE, "");
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.SUBTITLE);
        verify(mSubtitleView).setText("");
        verify(mSubtitleView).setVisibility(View.GONE);
    }

    @Test
    @SmallTest
    public void testStartIconBitmap() {
        Bitmap bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.START_ICON_BITMAP, bitmap)
                        .build();

        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.START_ICON_BITMAP);
        verify(mStartIcon).setImageDrawable(any(BitmapDrawable.class));
        verify(mStartIcon).setVisibility(View.VISIBLE);

        propertyModel.set(ListMenuItemProperties.START_ICON_BITMAP, null);
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.START_ICON_BITMAP);
        verify(mStartIcon).setImageDrawable(null);
        verify(mStartIcon).setVisibility(View.GONE);
    }

    @Test
    @SmallTest
    public void testStartIconBitmapWithKeepSpacing() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.KEEP_START_ICON_SPACING_WHEN_HIDDEN, true)
                        .with(ListMenuItemProperties.START_ICON_BITMAP, null)
                        .build();

        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.START_ICON_BITMAP);

        verify(mStartIcon).setImageDrawable(null);
        verify(mStartIcon).setVisibility(View.INVISIBLE);
    }

    @Test
    @SmallTest
    public void testEnabledState() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.ENABLED, false)
                        .build();
        ListMenuItemViewBinder.binder(propertyModel, mListItemView, ListMenuItemProperties.ENABLED);
        verify(mListItemView).setEnabled(false);
        verify(mTextView).setEnabled(false);
        verify(mStartIcon).setEnabled(false);
        verify(mEndIcon).setEnabled(false);

        propertyModel.set(ListMenuItemProperties.ENABLED, true);
        ListMenuItemViewBinder.binder(propertyModel, mListItemView, ListMenuItemProperties.ENABLED);
        verify(mListItemView).setEnabled(true);
        verify(mTextView).setEnabled(true);
        verify(mStartIcon).setEnabled(true);
        verify(mEndIcon).setEnabled(true);
    }

    @Test
    @SmallTest
    public void testIconTint() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(
                                ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID,
                                R.color.default_text_color_link_baseline)
                        .build();
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID);

        verify(mStartIcon).setImageTintList(any(ColorStateList.class));
        verify(mEndIcon).setImageTintList(any(ColorStateList.class));
        verify(mSubmenuArrow).setImageTintList(any(ColorStateList.class));

        propertyModel.set(ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID, Resources.ID_NULL);
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID);
        verify(mStartIcon).setImageTintList(null);
        verify(mEndIcon).setImageTintList(null);
        verify(mSubmenuArrow).setImageTintList(null);
    }

    @Test
    @SmallTest
    public void testIconTint_shouldNotTintEndIcon() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(
                                ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID,
                                R.color.default_text_color_link_baseline)
                        .with(ListMenuItemProperties.SHOULD_TINT_END_ICON, false)
                        .build();
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID);

        verify(mStartIcon).setImageTintList(any(ColorStateList.class));
        verify(mSubmenuArrow).setImageTintList(any(ColorStateList.class));
        // End icon should have its tint cleared (set to null)
        verify(mEndIcon).setImageTintList(null);
    }

    @Test
    @SmallTest
    public void testShouldTintEndIconProperty() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.SHOULD_TINT_END_ICON, false)
                        .build();
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.SHOULD_TINT_END_ICON);

        // End icon should have its tint cleared (set to null)
        verify(mEndIcon).setImageTintList(null);
    }

    @Test
    @SmallTest
    public void testShouldTintEndIconProperty_recyclesToTrue() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(
                                ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID,
                                R.color.default_text_color_link_baseline)
                        .with(ListMenuItemProperties.SHOULD_TINT_END_ICON, false)
                        .build();
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.SHOULD_TINT_END_ICON);

        // End icon should have its tint cleared (set to null)
        verify(mEndIcon).setImageTintList(null);

        // Set shouldTintEndIcon to true
        propertyModel.set(ListMenuItemProperties.SHOULD_TINT_END_ICON, true);
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.SHOULD_TINT_END_ICON);

        // Now end icon should be tinted using ICON_TINT_COLOR_STATE_LIST_ID
        verify(mEndIcon).setImageTintList(any(ColorStateList.class));
    }

    @Test
    @SmallTest
    public void testStartIconId() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.START_ICON_ID, R.drawable.ic_delete_fill_24dp)
                        .build();
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.START_ICON_ID);
        verify(mStartIcon).setImageDrawable(any(Drawable.class));
        verify(mStartIcon).setVisibility(View.VISIBLE);
        verify(mEndIcon, never()).setVisibility(anyInt());
    }

    @Test
    @SmallTest
    public void testEndIconId() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.END_ICON_ID, R.drawable.ic_delete_fill_24dp)
                        .build();
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.END_ICON_ID);
        verify(mEndIcon).setImageDrawable(any(Drawable.class));
        verify(mEndIcon).setVisibility(View.VISIBLE);
        verify(mStartIcon, never()).setVisibility(anyInt());
    }

    @Test
    @SmallTest
    public void testSubmenuHeaderIconTint() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS)
                        .with(
                                ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID,
                                R.color.default_text_color_link_baseline)
                        .build();

        // Verifies that ListMenuSubmenuHeaderViewBinder correctly tints all icons in the view.
        ListMenuSubmenuHeaderViewBinder.bind(
                propertyModel, mListItemView, ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID);

        verify(mStartIcon).setImageTintList(any(ColorStateList.class));
        verify(mEndIcon).setImageTintList(any(ColorStateList.class));
        verify(mSubmenuArrow).setImageTintList(any(ColorStateList.class));

        propertyModel.set(ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID, Resources.ID_NULL);
        ListMenuSubmenuHeaderViewBinder.bind(
                propertyModel, mListItemView, ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID);
        verify(mStartIcon).setImageTintList(null);
        verify(mEndIcon).setImageTintList(null);
        verify(mSubmenuArrow).setImageTintList(null);
    }

    @Test
    @SmallTest
    public void testStartIconWidth() {
        int width = 12;
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.START_ICON_WIDTH, width)
                        .build();

        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.START_ICON_WIDTH);

        verify(mStartIcon).getLayoutParams();
        verify(mStartIcon).setLayoutParams(mLayoutParams);
        assert (mLayoutParams.width == width);
    }

    @Test
    @SmallTest
    public void testBothIcons() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.START_ICON_ID, R.drawable.ic_delete_fill_24dp)
                        .with(ListMenuItemProperties.END_ICON_ID, R.drawable.ic_delete_fill_24dp)
                        .build();

        // Bind START_ICON_ID. It should show start icon and NOT hide end icon.
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.START_ICON_ID);
        verify(mStartIcon).setImageDrawable(any(Drawable.class));
        verify(mStartIcon).setVisibility(View.VISIBLE);
        verify(mEndIcon, never()).setVisibility(anyInt());

        // Bind END_ICON_ID. It should show end icon and NOT hide start icon.
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.END_ICON_ID);
        verify(mEndIcon).setImageDrawable(any(Drawable.class));
        verify(mEndIcon).setVisibility(View.VISIBLE);
        // Verify start icon wasn't touched again (total count remains 1).
        verify(mStartIcon).setVisibility(anyInt());
    }
}
