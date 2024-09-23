// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.text;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Typeface;
import android.text.TextPaint;

import androidx.core.content.res.ResourcesCompat;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.R;

/** Unit tests for {@link DownloadableFontTextAppearanceSpan}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class DownloadableFontTextAppearanceSpanTest {

    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();

    private Context mContext;
    private @Mock TextPaint mTextPaint;
    private @Mock Typeface mDefaultTypeface;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
    }

    @Test
    @SmallTest
    public void testTextAppearance() {
        DownloadableFontTextAppearanceSpan span =
                new DownloadableFontTextAppearanceSpan(
                        mContext, R.style.TextAppearance_Test_CustomFont);
        doReturn(mDefaultTypeface).when(mTextPaint).getTypeface();
        Typeface customTypeface = ResourcesCompat.getFont(mContext, R.font.custom_font);
        span.updateMeasureState(mTextPaint);
        verify(mTextPaint).setTypeface(customTypeface);

        Mockito.clearInvocations(mTextPaint);
        DownloadableFontTextAppearanceSpan spanWithInvalidTextAppearance =
                new DownloadableFontTextAppearanceSpan(mContext, 0);
        spanWithInvalidTextAppearance.updateMeasureState(mTextPaint);
        verify(mTextPaint, Mockito.never()).setTypeface(Mockito.any());
    }
}
