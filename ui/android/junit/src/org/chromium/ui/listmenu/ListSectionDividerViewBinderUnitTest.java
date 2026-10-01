// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import android.content.Context;
import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.R;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link ListSectionDividerViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ListSectionDividerViewBinderUnitTest {
    private Context mContext;
    private ViewGroup mDividerView;
    private View mDividerInternalView;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        mDividerView = new FrameLayout(mContext);
        mDividerInternalView = new View(mContext);
        mDividerInternalView.setId(R.id.divider_view);
        mDividerView.addView(mDividerInternalView);
    }

    @Test
    public void testColor() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListSectionDividerProperties.ALL_KEYS)
                        .with(ListSectionDividerProperties.COLOR_ID, R.color.divider_color_light)
                        .build();
        ListSectionDividerViewBinder.bind(
                propertyModel, mDividerView, ListSectionDividerProperties.COLOR_ID);

        Assert.assertEquals(
                mContext.getColor(R.color.divider_color_light),
                ((ColorDrawable) mDividerInternalView.getBackground()).getColor());
    }

    @Test
    public void testColor_DefaultOrZero() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListSectionDividerProperties.ALL_KEYS).build();
        ListSectionDividerViewBinder.bind(
                propertyModel, mDividerView, ListSectionDividerProperties.COLOR_ID);

        Assert.assertNull(mDividerInternalView.getBackground());
    }
}
