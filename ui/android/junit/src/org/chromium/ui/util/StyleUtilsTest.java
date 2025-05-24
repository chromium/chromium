// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.util;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.text.TextPaint;

import androidx.core.content.res.ResourcesCompat;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.ui.R;
import org.chromium.ui.util.StyleUtils.FontLoadingOutcome;

/** Tests for {@link StyleUtils} class. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class StyleUtilsTest {
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
    }

    @Test
    public void applyTextAppearanceToTextPaint_StringFontFamily() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.StyleUtils.FontLoadingOutcome", FontLoadingOutcome.FONT_FAMILY);
        TextPaint textPaint = new TextPaint();
        // Should not crash if font-family resource is not found, string font family will be
        // constructed instead.
        // This simulates default Chromium / OS behavior.
        StyleUtils.applyTextAppearanceToTextPaint(
                mContext,
                textPaint,
                R.style.TextAppearance_Test,
                /* applyFontFamily= */ true,
                /* applyTextSize= */ false,
                /* applyTextColor= */ false);
        watcher.assertExpected();
    }

    @Test
    public void applyTextAppearanceToTextPaint_CustomFontSizeText() {
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.StyleUtils.FontLoadingOutcome", FontLoadingOutcome.FONT_RES);
        TextPaint textPaint = new TextPaint();
        StyleUtils.applyTextAppearanceToTextPaint(
                mContext,
                textPaint,
                R.style.TextAppearance_Test_CustomFont,
                /* applyFontFamily= */ true,
                /* applyTextSize= */ true,
                /* applyTextColor= */ true);
        watcher.assertExpected();
        // Verify test values defined in resources.
        assertEquals(
                "Applied font is incorrect.",
                ResourcesCompat.getFont(mContext, R.font.custom_font),
                textPaint.getTypeface());
        assertEquals(
                "Applied text size incorrect.",
                mContext.getResources().getDimension(R.dimen.text_size_medium),
                textPaint.getTextSize(),
                0f);
        assertEquals(
                "Applied text color is incorrect.",
                mContext.getColor(R.color.default_text_color_light),
                textPaint.getColor());
    }
}
