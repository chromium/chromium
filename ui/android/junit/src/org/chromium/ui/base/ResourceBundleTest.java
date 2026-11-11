// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link ResourceBundle}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ResourceBundleTest {

    @Before
    public void setUp() {
        ResourceBundle.setAvailablePakLocalesForTesting(new String[] {"en-US", "fr-FR"});
    }

    @Test
    public void testGetLocalePakResourcePath_NullWhenLocalesEmpty() {
        ResourceBundle.setAvailablePakLocalesForTesting(new String[] {});
        assertNull(
                ResourceBundle.getLocalePakResourcePath(
                        "en-US", Gender.DEFAULT, /* inSplit= */ false, /* logError= */ false));
        assertNull(
                ResourceBundle.getLocalePakResourcePath(
                        "en-US", Gender.DEFAULT, /* inSplit= */ true, /* logError= */ false));
    }

    @Test
    public void testGetLocalePakResourcePath_NullForUnsupportedLocale() {
        assertNull(
                ResourceBundle.getLocalePakResourcePath(
                        "es-ES", Gender.DEFAULT, /* inSplit= */ false, /* logError= */ false));
        assertNull(
                ResourceBundle.getLocalePakResourcePath(
                        "es-ES", Gender.DEFAULT, /* inSplit= */ true, /* logError= */ false));
    }

    @Test
    public void testGetLocalePakResourcePath_NullWhenApkSubpathDoesNotExist() {
        assertNull(
                ResourceBundle.getLocalePakResourcePath(
                        "en-US", Gender.DEFAULT, /* inSplit= */ false, /* logError= */ false));
        assertNull(
                ResourceBundle.getLocalePakResourcePath(
                        "en-US", Gender.DEFAULT, /* inSplit= */ true, /* logError= */ false));
    }

    @Test
    public void testGetLocalePakResourcePath_ApkSubpathExists_DefaultGender() {
        ResourceBundle.setOverrideApkSubpathExistsForTesting(true);

        assertEquals(
                "assets/stored-locales/en-US.pak",
                ResourceBundle.getLocalePakResourcePath(
                        "en-US", Gender.DEFAULT, /* inSplit= */ false, /* logError= */ false));
        assertEquals(
                "assets/fallback-locales/en-US.pak",
                ResourceBundle.getLocalePakResourcePath(
                        "en-US", Gender.DEFAULT, /* inSplit= */ true, /* logError= */ false));
        assertEquals(
                "assets/locales#lang_fr/fr-FR.pak",
                ResourceBundle.getLocalePakResourcePath(
                        "fr-FR", Gender.DEFAULT, /* inSplit= */ true, /* logError= */ false));
    }

    @Test
    public void testGetLocalePakResourcePath_ApkSubpathExists_NonDefaultGender() {
        ResourceBundle.setOverrideApkSubpathExistsForTesting(true);

        assertEquals(
                "assets/stored-locales/en-US_FEMININE.pak",
                ResourceBundle.getLocalePakResourcePath(
                        "en-US", Gender.FEMININE, /* inSplit= */ false, /* logError= */ false));
        assertEquals(
                "assets/locales#lang_en/en-US_MASCULINE.pak",
                ResourceBundle.getLocalePakResourcePath(
                        "en-US", Gender.MASCULINE, /* inSplit= */ true, /* logError= */ false));
        assertEquals(
                "assets/locales#lang_fr/fr-FR_NEUTER.pak",
                ResourceBundle.getLocalePakResourcePath(
                        "fr-FR", Gender.NEUTER, /* inSplit= */ true, /* logError= */ false));
    }
}
