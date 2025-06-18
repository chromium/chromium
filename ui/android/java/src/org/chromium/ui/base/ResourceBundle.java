// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ApkAssets;
import org.chromium.base.LocaleUtils;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Arrays;

/**
 * This class provides the resource bundle related methods for the native
 * library.
 *
 * IMPORTANT: Clients that use {@link ResourceBundle} MUST call either
 * {@link ResourceBundle#setAvailablePakLocales(String[], String[])} or
 * {@link ResourceBundle#setNoAvailableLocalePaks()} before calling the getters in this class.
 */
@JNINamespace("ui")
@NullMarked
public final class ResourceBundle {
    private static final String TAG = "ResourceBundle";
    private static String @Nullable [] sAvailableLocales;
    private static boolean sOverrideApkSubpathExistsForTesting;
    private static boolean sOverrideFallbackPathExistsForTesting;

    private ResourceBundle() {}

    /** Called when there are no locale pak files available. */
    @CalledByNative
    public static void setNoAvailableLocalePaks() {
        assert sAvailableLocales == null;
        sAvailableLocales = new String[] {};
    }

    /**
     * Sets the available locale pak files.
     *
     * @param locales Locales that have pak files.
     */
    public static void setAvailablePakLocales(String[] locales) {
        assert sAvailableLocales == null;
        sAvailableLocales = locales;
    }

    public static void clearAvailablePakLocalesForTesting() {
        sAvailableLocales = null;
    }

    /**
     * Return the list of available locales.
     * @return The correct locale list for this build.
     */
    public static String[] getAvailableLocales() {
        assert sAvailableLocales != null;
        return sAvailableLocales;
    }

    /**
     * Return the location of a locale-specific .pak file asset.
     *
     * @param locale Chromium locale name.
     * @param gender User's gender.
     * @param inBundle If true, return the path of the uncompressed .pak file containing Chromium UI
     *     strings within app bundles. If false, return the path of the WebView UI strings instead.
     * @param logError Logs if the file is not found.
     * @return Asset path to .pak file, or null if the locale is not supported.
     */
    @CalledByNative
    static @Nullable String getLocalePakResourcePath(
            String locale, @Gender int gender, boolean inBundle, boolean logError) {
        if (sAvailableLocales == null) {
            // Locales may be null in unit tests.
            return null;
        }
        if (Arrays.binarySearch(sAvailableLocales, locale) < 0) {
            // This locale is not supported by Chromium.
            return null;
        }
        String pathPrefix = "assets/stored-locales/";
        if (inBundle) {
            if (locale.equals("en-US")) {
                pathPrefix = "assets/fallback-locales/";
            } else {
                String lang =
                        LocalizationUtils.getSplitLanguageForAndroid(
                                LocaleUtils.toBaseLanguage(locale));
                pathPrefix = "assets/locales#lang_" + lang + "/";
            }
        }
        String apkSubpath = maybeAppendGender(pathPrefix + locale, gender) + ".pak";
        // The file may not exist if the language split for this locale has not been installed
        // yet, so make sure it exists before returning the asset path.
        if (ApkAssets.exists(apkSubpath) || sOverrideApkSubpathExistsForTesting) {
            return apkSubpath;
        }
        // Fallback for apk targets.
        // TODO(crbug.com/40168285): Remove the need for this fallback logic.
        String fallbackPath = maybeAppendGender("assets/locales/" + locale, gender) + ".pak";
        if (ApkAssets.exists(fallbackPath) || sOverrideFallbackPathExistsForTesting) {
            return fallbackPath;
        }
        if (logError) {
            Log.e(TAG, "Did not exist: %s", apkSubpath);
        }
        return null;
    }

    /**
     * Sets sOverrideApkSubpathExistsForTesting.
     *
     * @param b Value to set.
     */
    static void setOverrideApkSubpathExistsForTesting(boolean b) {
        sOverrideApkSubpathExistsForTesting = b;
        ResettersForTesting.register(() -> sOverrideApkSubpathExistsForTesting = false);
    }

    /**
     * Sets sOverrideFallbackPathExistsForTesting.
     *
     * @param b Value to set.
     */
    static void setOverrideFallbackPathExistsForTesting(boolean b) {
        sOverrideFallbackPathExistsForTesting = b;
        ResettersForTesting.register(() -> sOverrideFallbackPathExistsForTesting = false);
    }

    /**
     * Appends a gender string to the given path if the gender is not the default.
     *
     * @param path The path to append to.
     * @param gender The gender to (maybe) append to the path.
     * @return A copy of the path, possibly with a gender suffix.
     */
    private static String maybeAppendGender(String path, @Gender int gender) {
        String suffix =
                switch (gender) {
                    case Gender.FEMININE -> "_FEMININE";
                    case Gender.MASCULINE -> "_MASCULINE";
                    case Gender.NEUTER -> "_NEUTER";
                    default -> "";
                };
        return path + suffix;
    }
}
