// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link ResourceBundle}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ResourceBundleTest {

    @Before
    public void setUp() {
        ResourceBundle.setAvailablePakLocales(new String[] {"en-US", "fr-FR"});
    }

    @After
    public void tearDown() {
        ResourceBundle.clearAvailablePakLocalesForTesting();
    }

    @Test
    public void testGetLocalePakResourcePath_NullWhenLocalesNotSet() {
        ResourceBundle.clearAvailablePakLocalesForTesting();
        assertNull(
                ResourceBundle.getLocalePakResourcePath(
                        "en-US", Gender.DEFAULT, /* inBundle= */ false, /* logError= */ false));
        assertNull(
                ResourceBundle.getLocalePakResourcePath(
                        "en-US", Gender.DEFAULT, /* inBundle= */ true, /* logError= */ false));
    }

    @Test
    public void testGetLocalePakResourcePath_NullForUnsupportedLocale() {
        assertNull(
                ResourceBundle.getLocalePakResourcePath(
                        "es-ES", Gender.DEFAULT, /* inBundle= */ false, /* logError= */ false));
        assertNull(
                ResourceBundle.getLocalePakResourcePath(
                        "es-ES", Gender.DEFAULT, /* inBundle= */ true, /* logError= */ false));
    }

    @Test
    public void testGetLocalePakResourcePath_NullWhenApkSubpathAndFallbackPathDoNotExist() {
        assertNull(
                ResourceBundle.getLocalePakResourcePath(
                        "en-US", Gender.DEFAULT, /* inBundle= */ false, /* logError= */ false));
        assertNull(
                ResourceBundle.getLocalePakResourcePath(
                        "en-US", Gender.DEFAULT, /* inBundle= */ true, /* logError= */ false));
    }

    @Test
    public void testGetLocalePakResourcePath_ApkSubpathExists_DefaultGender() {
        ResourceBundle.setOverrideApkSubpathExistsForTesting(true);

        assertEquals(
                "assets/stored-locales/en-US.pak",
                ResourceBundle.getLocalePakResourcePath(
                        "en-US", Gender.DEFAULT, /* inBundle= */ false, /* logError= */ false));
        assertEquals(
                "assets/fallback-locales/en-US.pak",
                ResourceBundle.getLocalePakResourcePath(
                        "en-US", Gender.DEFAULT, /* inBundle= */ true, /* logError= */ false));
        assertEquals(
                "assets/locales#lang_fr/fr-FR.pak",
                ResourceBundle.getLocalePakResourcePath(
                        "fr-FR", Gender.DEFAULT, /* inBundle= */ true, /* logError= */ false));
    }

    @Test
    public void testGetLocalePakResourcePath_ApkSubpathExists_NonDefaultGender() {
        ResourceBundle.setOverrideApkSubpathExistsForTesting(true);

        assertEquals(
                "assets/stored-locales/en-US_FEMININE.pak",
                ResourceBundle.getLocalePakResourcePath(
                        "en-US", Gender.FEMININE, /* inBundle= */ false, /* logError= */ false));
        assertEquals(
                "assets/fallback-locales/en-US_MASCULINE.pak",
                ResourceBundle.getLocalePakResourcePath(
                        "en-US", Gender.MASCULINE, /* inBundle= */ true, /* logError= */ false));
        assertEquals(
                "assets/locales#lang_fr/fr-FR_NEUTER.pak",
                ResourceBundle.getLocalePakResourcePath(
                        "fr-FR", Gender.NEUTER, /* inBundle= */ true, /* logError= */ false));
    }

    @Test
    public void testGetLocalePakResourcePath_FallbackPathExists_DefaultGender() {
        ResourceBundle.setOverrideFallbackPathExistsForTesting(true);

        assertEquals(
                "assets/locales/en-US.pak",
                ResourceBundle.getLocalePakResourcePath(
                        "en-US", Gender.DEFAULT, /* inBundle= */ false, /* logError= */ false));
        assertEquals(
                "assets/locales/en-US.pak",
                ResourceBundle.getLocalePakResourcePath(
                        "en-US", Gender.DEFAULT, /* inBundle= */ true, /* logError= */ false));
        assertEquals(
                "assets/locales/fr-FR.pak",
                ResourceBundle.getLocalePakResourcePath(
                        "fr-FR", Gender.DEFAULT, /* inBundle= */ true, /* logError= */ false));
    }

    @Test
    public void testGetLocalePakResourcePath_FallbackPathExists_NonDefaultGender() {
        ResourceBundle.setOverrideFallbackPathExistsForTesting(true);

        assertEquals(
                "assets/locales/en-US_FEMININE.pak",
                ResourceBundle.getLocalePakResourcePath(
                        "en-US", Gender.FEMININE, /* inBundle= */ false, /* logError= */ false));
        assertEquals(
                "assets/locales/en-US_MASCULINE.pak",
                ResourceBundle.getLocalePakResourcePath(
                        "en-US", Gender.MASCULINE, /* inBundle= */ true, /* logError= */ false));
        assertEquals(
                "assets/locales/fr-FR_NEUTER.pak",
                ResourceBundle.getLocalePakResourcePath(
                        "fr-FR", Gender.NEUTER, /* inBundle= */ true, /* logError= */ false));
    }
}
