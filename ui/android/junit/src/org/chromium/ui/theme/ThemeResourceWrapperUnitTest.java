// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.theme;

import static org.junit.Assert.assertEquals;

import android.graphics.Color;

import com.google.android.material.color.MaterialColors;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.R;
import org.chromium.ui.base.TestActivity;

@RunWith(BaseRobolectricTestRunner.class)
public class ThemeResourceWrapperUnitTest {

    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();

    private TestActivity mActivity;
    private ThemeResourceWrapper mThemeWrapper;

    @Mock private ThemeResourceWrapper.ThemeObserver mThemeObserver;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).create().get();
        mActivity.setTheme(R.style.ThemeOverlay_BlackPrimary);

        mThemeWrapper = new ThemeResourceWrapper(mActivity, R.style.ThemeOverlay_WhitePrimary);
        mThemeWrapper.addObserver(mThemeObserver);
    }

    @After
    public void tearDown() {
        mThemeWrapper.destroy();
        mActivity.finish();
    }

    @Test
    public void testThemeWrapper() {
        mThemeWrapper.setIsUsingOverlay(true);
        Mockito.verify(mThemeObserver).onThemeResourceChanged(mThemeWrapper);

        int colorPrimary = MaterialColors.getColor(mActivity, R.attr.colorPrimary, "TAG");
        int wrapperColorPrimary =
                MaterialColors.getColor(
                        mThemeWrapper.getThemedContextForTesting(), R.attr.colorPrimary, "TAG");

        assertEquals("Primary expected to be black without overlay.", Color.BLACK, colorPrimary);
        assertEquals(
                "Themed context should have different color.", Color.WHITE, wrapperColorPrimary);
    }

    @Test
    public void testThemeWrapper_Disabled() {
        mThemeWrapper.setIsUsingOverlay(false);
        Mockito.verifyNoInteractions(mThemeObserver);

        int colorPrimary = MaterialColors.getColor(mActivity, R.attr.colorPrimary, "TAG");
        int wrapperColorPrimary =
                MaterialColors.getColor(
                        mThemeWrapper.getThemedContextForTesting(), R.attr.colorPrimary, "TAG");

        assertEquals("Primary expected to be black without overlay.", Color.BLACK, colorPrimary);
        assertEquals(
                "Themed context should have different color.", Color.BLACK, wrapperColorPrimary);
    }
}
