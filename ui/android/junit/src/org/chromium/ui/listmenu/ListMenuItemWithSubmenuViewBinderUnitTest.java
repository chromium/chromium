// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.R;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link ListMenuItemWithSubmenuViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ListMenuItemWithSubmenuViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View.OnTouchListener mOnTouchListener;
    @Mock private View.OnGenericMotionListener mOnGenericMotionListener;

    private Context mContext;
    private ViewGroup mListItemView;
    private TextView mTextView;
    private ImageView mStartIcon;
    private ImageView mSubmenuArrow;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        mListItemView = new LinearLayout(mContext);
        mStartIcon = new ImageView(mContext);
        mStartIcon.setId(R.id.menu_item_icon);
        mTextView = new TextView(mContext);
        mTextView.setId(R.id.menu_row_text);
        mSubmenuArrow = new ImageView(mContext);
        mSubmenuArrow.setId(R.id.submenu_arrow);

        mListItemView.addView(mStartIcon);
        mListItemView.addView(mTextView);
        mListItemView.addView(mSubmenuArrow);
    }

    @Test
    public void testIconTint() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS)
                        .with(
                                ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID,
                                R.color.default_text_color_link_baseline)
                        .build();
        ListMenuItemWithSubmenuViewBinder.bind(
                propertyModel, mListItemView, ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID);

        ColorStateList expectedTint =
                mContext.getColorStateList(R.color.default_text_color_link_baseline);
        Assert.assertEquals(expectedTint, mStartIcon.getImageTintList());
        Assert.assertEquals(expectedTint, mSubmenuArrow.getImageTintList());

        propertyModel.set(ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID, Resources.ID_NULL);
        ListMenuItemWithSubmenuViewBinder.bind(
                propertyModel, mListItemView, ListMenuItemProperties.ICON_TINT_COLOR_STATE_LIST_ID);
        Assert.assertNull(mStartIcon.getImageTintList());
        Assert.assertNull(mSubmenuArrow.getImageTintList());
    }

    @Test
    public void testTouchListener() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS)
                        .with(ListMenuItemProperties.TOUCH_LISTENER, mOnTouchListener)
                        .build();
        ListMenuItemWithSubmenuViewBinder.bind(
                propertyModel, mListItemView, ListMenuItemProperties.TOUCH_LISTENER);
        MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_DOWN, 0, 0, 0);
        mListItemView.dispatchTouchEvent(event);
        verify(mOnTouchListener).onTouch(mListItemView, event);
    }

    @Test
    public void testGenericMotionListener() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListMenuSubmenuItemProperties.ALL_KEYS)
                        .with(
                                ListMenuItemProperties.GENERIC_MOTION_LISTENER,
                                mOnGenericMotionListener)
                        .build();
        ListMenuItemWithSubmenuViewBinder.bind(
                propertyModel, mListItemView, ListMenuItemProperties.GENERIC_MOTION_LISTENER);
        MotionEvent event = MotionEvent.obtain(0, 0, MotionEvent.ACTION_BUTTON_PRESS, 0, 0, 0);
        mListItemView.dispatchGenericMotionEvent(event);
        verify(mOnGenericMotionListener).onGenericMotion(mListItemView, event);
    }
}
