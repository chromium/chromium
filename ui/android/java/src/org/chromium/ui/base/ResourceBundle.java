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
import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Arrays;

/** This class provides the resource bundle related methods for the native library. */
@JNINamespace("ui")
@NullMarked
public final class ResourceBundle {
    private static final String TAG = "ResourceBundle";
    private static String[] sAvailableLocales = BuildConfig.LOCALES;
    private static boolean sOverrideApkSubpathExistsForTesting;

    private ResourceBundle() {}

    /**
     * Sets the available locale pak files for testing.
     *
     * @param locales Locales that have pak files.
     */
    public static void setAvailablePakLocalesForTesting(String[] locales) {
        sAvailableLocales = locales;
        ResettersForTesting.register(() -> sAvailableLocales = BuildConfig.LOCALES);
    }

    /**
     * Return the list of available locales.
     *
     * @return The correct locale list for this build.
     */
    public static String[] getAvailableLocales() {
        return sAvailableLocales;
    }

    /**
     * Return the location of a locale-specific .pak file asset.
     *
     * @param locale Chromium locale name.
     * @param gender User's gender.
     * @param inSplit If true, return the path of the .pak file containing Chromium UI strings,
     *     which is stored in a language split when using app bundles. If false, return the path of
     *     the WebView UI strings instead.
     * @param logError Logs if the file is not found.
     * @return Asset path to .pak file, or null if the locale is not supported.
     */
    @CalledByNative
    static @Nullable String getLocalePakResourcePath(
            String locale, @Gender int gender, boolean inSplit, boolean logError) {
        if (Arrays.binarySearch(sAvailableLocales, locale) < 0) {
            // This locale is not supported by Chromium.
            return null;
        }
        String pathPrefix;
        if (!inSplit) {
            pathPrefix = "assets/stored-locales/";
        } else if (locale.equals("en-US") && gender == Gender.DEFAULT) {
            pathPrefix = "assets/fallback-locales/";
        } else {
            String lang =
                    LocalizationUtils.getSplitLanguageForAndroid(
                            LocaleUtils.toBaseLanguage(locale));
            pathPrefix = "assets/locales#lang_" + lang + "/";
        }
        String genderSuffix =
                switch (gender) {
                    case Gender.FEMININE -> "_FEMININE";
                    case Gender.MASCULINE -> "_MASCULINE";
                    case Gender.NEUTER -> "_NEUTER";
                    default -> "";
                };
        String apkSubpath = pathPrefix + locale + genderSuffix + ".pak";
        // The file may not exist if the language split for this locale has not been installed
        // yet, so make sure it exists before returning the asset path.
        if (ApkAssets.exists(apkSubpath) || sOverrideApkSubpathExistsForTesting) {
            return apkSubpath;
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
}
