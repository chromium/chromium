// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;
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

    @Mock private View mListItemView;
    @Mock private TextView mTextView;
    @Mock private ImageView mStartIcon;
    @Mock private ImageView mEndIcon;
    @Mock private TextView mSubtitleView;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        when(mListItemView.getContext()).thenReturn(mContext);
        when(mListItemView.getResources()).thenReturn(mContext.getResources());
        when(mListItemView.findViewById(R.id.menu_item_text)).thenReturn(mTextView);
        when(mListItemView.findViewById(R.id.menu_item_icon)).thenReturn(mStartIcon);
        when(mListItemView.findViewById(R.id.menu_item_end_icon)).thenReturn(mEndIcon);
        when(mListItemView.findViewById(R.id.menu_item_subtitle)).thenReturn(mSubtitleView);
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

        propertyModel.set(ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID, 0);
        ListMenuItemViewBinder.binder(
                propertyModel, mListItemView, ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID);
        verify(mStartIcon).setImageTintList(null);
        verify(mEndIcon).setImageTintList(null);
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
        verify(mEndIcon).setVisibility(View.GONE);
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
        verify(mStartIcon).setVisibility(View.GONE);
    }
}
