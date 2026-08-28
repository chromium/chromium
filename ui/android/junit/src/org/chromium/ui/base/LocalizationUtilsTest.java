// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.Locale;

/** Tests for LocalizationUtils class. */
@RunWith(BaseRobolectricTestRunner.class)
public class LocalizationUtilsTest {
    @Test
    @SmallTest
    public void testGetSplitLanguageForAndroid() {
        assertEquals("en", LocalizationUtils.getSplitLanguageForAndroid("en"));
        assertEquals("es", LocalizationUtils.getSplitLanguageForAndroid("es"));
        assertEquals("fr", LocalizationUtils.getSplitLanguageForAndroid("fr"));
        assertEquals("iw", LocalizationUtils.getSplitLanguageForAndroid("he"));
        assertEquals("ji", LocalizationUtils.getSplitLanguageForAndroid("yi"));
        assertEquals("tl", LocalizationUtils.getSplitLanguageForAndroid("fil"));
    }

    @Test
    @SmallTest
    public void testChromiumLocaleMatchesLanguage() {
        assertTrue(LocalizationUtils.chromiumLocaleMatchesLanguage("en-US", "en"));
        assertTrue(LocalizationUtils.chromiumLocaleMatchesLanguage("en-GB", "en"));
        assertFalse(LocalizationUtils.chromiumLocaleMatchesLanguage("en-US", "es"));
        assertTrue(LocalizationUtils.chromiumLocaleMatchesLanguage("es", "es"));
        assertTrue(LocalizationUtils.chromiumLocaleMatchesLanguage("fi", "fi"));

        // Filipino locale should *not* match Finish language.
        // See http://crbug.com/901837
        assertFalse(LocalizationUtils.chromiumLocaleMatchesLanguage("fil", "fi"));

        // "tl" is the Android locale name for Filipines, due to historical
        // reasons. The corresponding Chromium locale name is "fil".
        // Check that the method only deals with Chromium locale names.
        assertFalse(LocalizationUtils.chromiumLocaleMatchesLanguage("fil", "tl"));
    }

    @Test
    @SmallTest
    public void testGetJavaLocaleForBcp47Tag() {
        Locale enUs = LocalizationUtils.getJavaLocaleForBcp47Tag("en-US");
        assertEquals("en", enUs.getLanguage());
        assertEquals("US", enUs.getCountry());

        Locale jaJp = LocalizationUtils.getJavaLocaleForBcp47Tag("ja-JP");
        assertEquals("ja", jaJp.getLanguage());
        assertEquals("JP", jaJp.getCountry());

        Locale zhHans = LocalizationUtils.getJavaLocaleForBcp47Tag("zh-Hans-CN");
        assertEquals("zh", zhHans.getLanguage());
        assertEquals("CN", zhHans.getCountry());
        assertEquals("Hans", zhHans.getScript());
    }
}
