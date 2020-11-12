// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.content.res.AssetFileDescriptor;
import android.content.res.AssetManager;

import org.chromium.base.ContextUtils;
import org.chromium.base.LocaleUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.io.IOException;
import java.util.Arrays;

/**
 * This class provides the resource bundle related methods for the native
 * library.
 *
 * IMPORTANT: Clients that use {@link ResourceBundle} and/or
 * {@link org.chromium.ui.resources.ResourceExtractor} MUST call either
 * {@link ResourceBundle#setAvailablePakLocales(String[], String[])} or
 * {@link ResourceBundle#setNoAvailableLocalePaks()} before calling the getters in this class.
 */
@JNINamespace("ui")
public final class ResourceBundle {
    private static final String TAG = "ResourceBundle";
    private static String[] sCompressedLocales;
    private static String[] sUncompressedLocales;

    private ResourceBundle() {}

    /**
     * Called when there are no locale pak files available.
     */
    @CalledByNative
    public static void setNoAvailableLocalePaks() {
        assert sCompressedLocales == null && sUncompressedLocales == null;
        sCompressedLocales = new String[] {};
        sUncompressedLocales = new String[] {};
    }

    /**
     * Sets the available compressed and uncompressed locale pak files.
     * @param compressed Locales that have compressed pak files.
     * @param uncompressed Locales that have uncompressed pak files.
     */
    public static void setAvailablePakLocales(String[] compressed, String[] uncompressed) {
        assert sCompressedLocales == null && sUncompressedLocales == null;
        sCompressedLocales = compressed;
        sUncompressedLocales = uncompressed;
    }

    /**
     * Return the array of locales that have compressed pak files. Do not modify the array.
     * @return The locales that have compressed pak files.
     */
    public static String[] getAvailableCompressedPakLocales() {
        assert sCompressedLocales != null;
        return sCompressedLocales;
    }

    /**
     * Return the location of a locale-specific uncompress .pak file asset.
     *
     * @param locale Chromium locale name.
     * @param inBundle If true, return the path of the uncompressed .pak file
     *                 containing Chromium UI strings within app bundles. If
     *                 false, return the path of the uncompressed WebView UI
     *                 strings instead. Note that APK .pak files are stored
     *                 compressed and handled differently.
     * @param logError Logs if the file is not found.
     * @return Asset path to uncompressed .pak file, or null if the locale is
     *         not supported by this version of Chromium, or the file is
     *         missing.
     */
    @CalledByNative
    private static String getLocalePakResourcePath(
            String locale, boolean inBundle, boolean logError) {
        if (sUncompressedLocales == null) {
            // Locales may be null in unit tests.
            return null;
        }
        if (Arrays.binarySearch(sUncompressedLocales, locale) < 0) {
            // This locale is not supported by Chromium.
            return null;
        }
        String pathPrefix = "assets/stored-locales/";
        if (inBundle) {
            if (locale.equals("en-US")) {
                pathPrefix = "assets/fallback-locales/";
            } else {
                String lang = LocalizationUtils.getSplitLanguageForAndroid(
                        LocaleUtils.toLanguage(locale));
                pathPrefix = "assets/locales#lang_" + lang + "/";
            }
        }
        String assetPath = pathPrefix + locale + ".pak";
        AssetManager manager = ContextUtils.getApplicationContext().getAssets();
        // The file may not exist if the language split for this locale has not been installed
        // yet, so make sure it exists before returning the asset path.
        try (AssetFileDescriptor afd = manager.openNonAssetFd(assetPath)) {
            return assetPath;
        } catch (IOException e) {
            if (logError) {
                Log.e(TAG, "path=%s", assetPath, e);
            }
            return null;
        }
    }
}
