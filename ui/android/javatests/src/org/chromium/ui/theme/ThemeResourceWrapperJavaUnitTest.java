// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.theme;

import static org.junit.Assert.assertEquals;

import android.content.res.AssetManager;
import android.content.res.Resources;
import android.content.res.Resources.Theme;
import android.graphics.Color;
import android.os.Bundle;
import android.util.TypedValue;

import androidx.annotation.AttrRes;
import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.ui.R;

/** Java test show case how to use the wrapper. */
@Batch(Batch.PER_CLASS)
@RunWith(BaseJUnit4ClassRunner.class)
public class ThemeResourceWrapperJavaUnitTest {

    @Rule
    public BaseActivityTestRule<ThemeResourceTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(ThemeResourceTestActivity.class);

    private ThemeResourceTestActivity mActivity;

    @Before
    public void setup() {
        mActivityTestRule.launchActivity(null);
        mActivity = mActivityTestRule.getActivity();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivity.getThemeResourceWrapper().setIsUsingOverlay(false);
                });
    }

    @Test
    @SmallTest
    public void setThemeForOriginalActivity() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "Color without overlay is blue.",
                            mActivity.getColor(android.R.color.holo_blue_light),
                            getPrimaryColor());

                    mActivity.setTheme(R.style.ThemeOverlay_WhitePrimary);
                    assertEquals(
                            "Color with new overlay will override the activity's default.",
                            Color.WHITE,
                            getPrimaryColor());
                });
    }

    @Test
    @SmallTest
    public void setThemeNoOpWithThemeResourceWrapper() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "Color without overlay is blue.",
                            mActivity.getColor(android.R.color.holo_blue_light),
                            getPrimaryColor());

                    mActivity.getThemeResourceWrapper().setIsUsingOverlay(true);
                    assertEquals(
                            "Color using resource wrapper will be override into black.",
                            Color.BLACK,
                            getPrimaryColor());

                    mActivity.setTheme(R.style.ThemeOverlay_WhitePrimary);
                    assertEquals(
                            "While theme resource wrapper is in use, setTheme for activity is no"
                                    + " op.",
                            Color.BLACK,
                            getPrimaryColor());
                });
    }

    @Test
    @SmallTest
    public void themeWrapperNoImpactOnOtherAttribute() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(
                            "Text color highlight without overlay is blue.",
                            mActivity.getColor(android.R.color.holo_blue_light),
                            getTextColorHighlight());

                    mActivity.getThemeResourceWrapper().setIsUsingOverlay(true);
                    assertEquals(
                            "Text color highlight should remain no impacted by"
                                    + " ThemeResourceWrapper.",
                            mActivity.getColor(android.R.color.holo_blue_light),
                            getTextColorHighlight());

                    // As theme resource is created by the time ThemeResourceWrapper is created, the
                    // new overlay applied to the base theme will not impact the overlay.
                    mActivity.setTheme(R.style.ThemeOverlay_WhitePrimary_WithTextColor);
                    assertEquals(
                            "Text color highlight should should remain no impacted.",
                            mActivity.getColor(android.R.color.holo_blue_light),
                            getTextColorHighlight());

                    // Turning off the overlay, the text color overlay that applied to the base
                    // activity will take effect.
                    mActivity.getThemeResourceWrapper().setIsUsingOverlay(false);
                    assertEquals(
                            "Text color highlight is override by #setTheme.",
                            Color.WHITE,
                            getTextColorHighlight());
                });
    }

    private @ColorInt int getPrimaryColor() {
        return getColorFromAttr(R.attr.colorPrimary);
    }

    private @ColorInt int getTextColorHighlight() {
        return getColorFromAttr(android.R.attr.textColorHighlight);
    }

    private @ColorInt int getColorFromAttr(@AttrRes int attrRes) {
        TypedValue tv = new TypedValue();
        boolean success = mActivity.getTheme().resolveAttribute(attrRes, tv, true);

        return success ? mActivity.getColor(tv.resourceId) : Color.TRANSPARENT;
    }

    /**
     * Basic activity that delegates the resource call to a {@link ThemeResourceWrapper} instance.
     */
    public static class ThemeResourceTestActivity extends AppCompatActivity {

        private ThemeResourceWrapper mThemeResourceWrapper;

        public ThemeResourceWrapper getThemeResourceWrapper() {
            return mThemeResourceWrapper;
        }

        @Override
        protected void onCreate(Bundle savedInstanceState) {
            super.onCreate(savedInstanceState);
            setTheme(R.style.ThemeOverlay_BluePrimary);
            mThemeResourceWrapper =
                    new ThemeResourceWrapper(this, R.style.ThemeOverlay_BlackPrimary);
        }

        @Override
        public Resources getResources() {
            if (mThemeResourceWrapper != null && !mThemeResourceWrapper.isBusy()) {
                return mThemeResourceWrapper.getResources();
            }
            return super.getResources();
        }

        @Override
        public Theme getTheme() {
            if (mThemeResourceWrapper != null && !mThemeResourceWrapper.isBusy()) {
                return mThemeResourceWrapper.getTheme();
            }
            return super.getTheme();
        }

        @Override
        public AssetManager getAssets() {
            if (mThemeResourceWrapper != null && !mThemeResourceWrapper.isBusy()) {
                return mThemeResourceWrapper.getAssets();
            }
            return super.getAssets();
        }

        @Override
        public Object getSystemService(@NonNull String name) {
            if (mThemeResourceWrapper != null && !mThemeResourceWrapper.isBusy()) {
                return mThemeResourceWrapper.getSystemService(name);
            }
            return super.getSystemService(name);
        }
    }
}
