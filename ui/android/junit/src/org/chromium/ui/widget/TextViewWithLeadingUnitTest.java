// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import static org.junit.Assert.assertNotEquals;

import android.content.Context;
import android.view.InflateException;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.LayoutRes;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.ui.R;
import org.chromium.ui.base.UiAndroidFeatures;

/** Unit tests for {@link TextViewWithLeading}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TextViewWithLeadingUnitTest {
    private void inflateAndVerify(@LayoutRes int layoutRes) {
        TextView textView = (TextView) inflate(layoutRes);
        assertNotEquals(/* unexpected= */ 0, textView.getLineSpacingExtra(), /* delta= */ 1);
    }

    private View inflate(@LayoutRes int layoutRes) {
        Context context = ApplicationProvider.getApplicationContext();
        return LayoutInflater.from(context).inflate(layoutRes, /* root= */ null);
    }

    @Test
    public void testDirect() {
        inflateAndVerify(R.layout.text_view_with_leading_direct);
    }

    @Test
    public void testStyle() {
        inflateAndVerify(R.layout.text_view_with_leading_style);
    }

    @Test
    public void testTextAppearance() {
        inflateAndVerify(R.layout.text_view_with_leading_text_appearance);
    }

    @Test
    @DisableFeatures(UiAndroidFeatures.REQUIRE_LEADING_IN_TEXT_VIEW_WITH_LEADING)
    public void testBadTextAppearance() {
        inflate(R.layout.text_view_with_leading_bad_text_appearance);
    }

    @Test
    public void testStyleIntoTextAppearance() {
        inflateAndVerify(R.layout.text_view_with_leading_style_into_text_appearance);
    }

    @Test(expected = InflateException.class)
    @EnableFeatures(UiAndroidFeatures.REQUIRE_LEADING_IN_TEXT_VIEW_WITH_LEADING)
    public void testNoLeading() {
        inflate(R.layout.text_view_with_leading_no_leading);
    }

    @Test
    @DisableFeatures(UiAndroidFeatures.REQUIRE_LEADING_IN_TEXT_VIEW_WITH_LEADING)
    public void testLeadingKillSwitch() {
        inflateAndVerify(R.layout.text_view_with_leading_direct);
    }

    @Test
    @DisableFeatures(UiAndroidFeatures.REQUIRE_LEADING_IN_TEXT_VIEW_WITH_LEADING)
    public void testNoLeadingKillSwitch() {
        inflate(R.layout.text_view_with_leading_no_leading);
    }
}
