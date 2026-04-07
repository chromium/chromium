// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.listmenu;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.View;

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

/** Tests for {@link ListSectionDividerViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ListSectionDividerViewBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View mDividerView;
    @Mock private View mDividerInternalView;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = RuntimeEnvironment.application;
        when(mDividerView.getContext()).thenReturn(mContext);
        when(mDividerView.findViewById(R.id.divider_view)).thenReturn(mDividerInternalView);
    }

    @Test
    @SmallTest
    public void testColor() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(ListSectionDividerProperties.ALL_KEYS)
                        .with(ListSectionDividerProperties.COLOR_ID, R.color.divider_color_light)
                        .build();
        ListSectionDividerViewBinder.bind(
                propertyModel, mDividerView, ListSectionDividerProperties.COLOR_ID);

        verify(mDividerInternalView)
                .setBackgroundColor(mContext.getColor(R.color.divider_color_light));
    }
}
