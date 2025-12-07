// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.theme;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.content.Context;
import android.util.TypedValue;

import androidx.annotation.AttrRes;
import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.R;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link FillInContextThemeWrapper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class FillInContextThemeWrapperUnitTest {
    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    private Activity mActivity;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
    }

    @Test
    public void testApplyMissingAttributes() {
        mActivity.setTheme(R.style.ThemeOverlay_TextLarge_SizeOnly);
        Context wrappedContext =
                new FillInContextThemeWrapper(mActivity, R.style.ThemeOverlay_TextLarge_Medium);

        int expectedTextLargeLeading =
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.text_size_large_leading_desktop);
        int textLargeLeading = getDimensionPixelSize(wrappedContext, R.attr.textLargeLeading);

        assertEquals("Missing attr should be applied.", expectedTextLargeLeading, textLargeLeading);
    }

    @Test
    public void testDoesNotOverrideExistingAttributes() {
        mActivity.setTheme(R.style.ThemeOverlay_TextLarge_SizeOnly);
        Context wrappedContext =
                new FillInContextThemeWrapper(mActivity, R.style.ThemeOverlay_TextLarge_Medium);

        int ogTextLargeSize =
                mActivity.getResources().getDimensionPixelSize(R.dimen.text_size_large);
        int textLargeSize = getDimensionPixelSize(wrappedContext, R.attr.textLargeSize);

        assertEquals("Existing attr shouldn't be overridden.", ogTextLargeSize, textLargeSize);
    }

    private static int getDimensionPixelSize(Context context, @AttrRes int dimen) {
        var typedValue = new TypedValue();

        if (context.getTheme().resolveAttribute(dimen, typedValue, true)) {
            return TypedValue.complexToDimensionPixelSize(
                    typedValue.data, context.getResources().getDisplayMetrics());
        }
        return -1;
    }
}
