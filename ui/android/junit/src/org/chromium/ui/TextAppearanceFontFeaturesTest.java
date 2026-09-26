// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.res.TypedArray;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Tests that ligature-forming OpenType features stay disabled for native UI text.
 *
 * <p>Some fonts render a run of ordinary characters as a single unrelated glyph, which is not
 * appropriate for the untrusted text that browser UI has to display. The root TextAppearance style
 * switches those features off, and every other text style in the UI inherits from it. These tests
 * guard that wiring: they fail if the item is dropped from the root style, if a style is
 * re-parented away from the root, or if the feature list itself is weakened.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class TextAppearanceFontFeaturesTest {
    private static final int[] FONT_FEATURE_SETTINGS = {android.R.attr.fontFeatureSettings};

    private Context mContext;
    private String mExpectedSettings;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        mExpectedSettings = mContext.getString(R.string.default_font_feature_settings);
    }

    /** Returns the android:fontFeatureSettings that {@code styleRes} resolves to, or null. */
    private String fontFeatureSettingsOf(int styleRes) {
        TypedArray a = mContext.getTheme().obtainStyledAttributes(styleRes, FONT_FEATURE_SETTINGS);
        try {
            return a.getString(0);
        } finally {
            a.recycle();
        }
    }

    @Test
    public void rootTextAppearanceDisablesLigatures() {
        assertEquals(
                "The root TextAppearance style must set android:fontFeatureSettings. Every"
                        + " TextView in the UI inherits the setting from here, so dropping it"
                        + " re-enables ligatures across the whole browser UI.",
                mExpectedSettings,
                fontFeatureSettingsOf(R.style.TextAppearance));
    }

    @Test
    public void descendantStylesInheritFontFeatureSettings() {
        // Sampled from across the hierarchy, including the two styles that select a non-default
        // font. If any of these stops matching, that branch has been re-parented away from the
        // root TextAppearance and no longer inherits the setting.
        assertEquals(
                "TextAppearance.TextLarge",
                mExpectedSettings,
                fontFeatureSettingsOf(R.style.TextAppearance_TextLarge));
        assertEquals(
                "TextAppearance.TextMedium",
                mExpectedSettings,
                fontFeatureSettingsOf(R.style.TextAppearance_TextMedium));
        assertEquals(
                "TextAppearance.Headline",
                mExpectedSettings,
                fontFeatureSettingsOf(R.style.TextAppearance_Headline));
        assertEquals(
                "TextAppearance.AccentMediumStyle",
                mExpectedSettings,
                fontFeatureSettingsOf(R.style.TextAppearance_AccentMediumStyle));
        assertEquals(
                "TextAppearance.TextAccentLarge",
                mExpectedSettings,
                fontFeatureSettingsOf(R.style.TextAppearance_TextAccentLarge));
    }

    @Test
    public void bothLigatureFeaturesAreDisabled() {
        // Guards the value itself rather than the wiring. Both features are required: each one
        // alone leaves part of the problem unfixed, because a font can form the same glyph
        // through either mechanism.
        assertNotNull(mExpectedSettings);
        assertTrue(
                "Standard ligatures must be disabled, got: " + mExpectedSettings,
                mExpectedSettings.contains("\"liga\" 0"));
        assertTrue(
                "Contextual alternates must be disabled, got: " + mExpectedSettings,
                mExpectedSettings.contains("\"calt\" 0"));
    }

    @Test
    public void requiredShapingFeaturesAreNotDisabled() {
        // Required ligatures and the joining/conjunct features are what make cursive and complex
        // scripts render correctly. Disabling any of them would break Arabic, Indic and similar
        // scripts, so they must never be added to this list.
        for (String feature :
                new String[] {
                    "rlig", "isol", "init", "medi", "fina", "akhn", "half", "rphf", "pref", "blwf",
                    "pstf", "vatu", "cjct"
                }) {
            assertFalse(
                    "Feature '"
                            + feature
                            + "' is required for correct script shaping and must not be disabled,"
                            + " but the settings are: "
                            + mExpectedSettings,
                    mExpectedSettings.contains(feature));
        }
    }
}
