// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
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

/** Tests for {@link ListMenuItemWithSubmenuViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ListMenuItemWithSubmenuViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View mListItemView;
    @Mock private TextView mTextView;
    @Mock private ImageView mStartIcon;
    @Mock private ImageView mSubmenuArrow;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        when(mListItemView.getContext()).thenReturn(mContext);
        when(mListItemView.getResources()).thenReturn(mContext.getResources());
        when(mListItemView.findViewById(R.id.menu_row_text)).thenReturn(mTextView);
        when(mListItemView.findViewById(R.id.menu_item_icon)).thenReturn(mStartIcon);
        when(mListItemView.findViewById(R.id.submenu_arrow)).thenReturn(mSubmenuArrow);
    }

    @Test
    @SmallTest
    public void testIconTint() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS)
                        .with(
                                ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID,
                                R.color.default_text_color_link_baseline)
                        .build();
        ListMenuItemWithSubmenuViewBinder.bind(
                propertyModel, mListItemView, ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID);

        verify(mStartIcon).setImageTintList(any(ColorStateList.class));
        verify(mSubmenuArrow).setImageTintList(any(ColorStateList.class));

        propertyModel.set(ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID, Resources.ID_NULL);
        ListMenuItemWithSubmenuViewBinder.bind(
                propertyModel, mListItemView, ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID);
        verify(mStartIcon).setImageTintList(null);
        verify(mSubmenuArrow).setImageTintList(null);
    }
}
