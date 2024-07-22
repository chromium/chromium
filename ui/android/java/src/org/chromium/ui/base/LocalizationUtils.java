// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.view.View;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.LocaleUtils;
import org.chromium.base.ResettersForTesting;

import java.util.Locale;

/** This class provides the locale related methods for the native library. */
@JNINamespace("l10n_util")
public class LocalizationUtils {

    // This is mirrored from base/i18n/rtl.h. Please keep in sync.
    public static final int UNKNOWN_DIRECTION = 0;
    public static final int RIGHT_TO_LEFT = 1;
    public static final int LEFT_TO_RIGHT = 2;

    private static Boolean sIsLayoutRtlForTesting;

    private LocalizationUtils() {
        /* cannot be instantiated */
    }

    @CalledByNative
    private static Locale getJavaLocale(String language, String country, String variant) {
        return new Locale(language, country, variant);
    }

    @CalledByNative
    private static String getDisplayNameForLocale(Locale locale, Locale displayLocale) {
        return locale.getDisplayName(displayLocale);
    }

    /**
     * Returns whether the Android layout direction is RTL.
     *
     * Note that the locale direction can be different from layout direction. Two known cases:
     * - RTL languages on Android 4.1, due to the lack of RTL layout support on 4.1.
     * - When user turned on force RTL layout option under developer options.
     *
     * Therefore, only this function should be used to query RTL for layout purposes.
     */
    @CalledByNative
    public static boolean isLayoutRtl() {
        if (sIsLayoutRtlForTesting != null) return sIsLayoutRtlForTesting;

        return ContextUtils.getApplicationContext()
                        .getResources()
                        .getConfiguration()
                        .getLayoutDirection()
                == View.LAYOUT_DIRECTION_RTL;
    }

    @CalledByNativeForTesting
    public static void setRtlForTesting(boolean shouldBeRtl) {
        sIsLayoutRtlForTesting = shouldBeRtl;
        ResettersForTesting.register(() -> sIsLayoutRtlForTesting = null);
    }

    /** Returns whether navigation gestures should be mirrored due to the UI language. */
    @CalledByNative
    public static boolean shouldMirrorBackForwardGestures() {
        if (!UiAndroidFeatureMap.isEnabled(UiAndroidFeatures.MIRROR_BACK_FORWARD_GESTURES_IN_RTL)) {
            return false;
        }

        return LocalizationUtils.isLayoutRtl();
    }

    /**
     * Jni binding to base::i18n::GetFirstStrongCharacterDirection
     *
     * @param string String to decide the direction.
     * @return One of the UNKNOWN_DIRECTION, RIGHT_TO_LEFT, and LEFT_TO_RIGHT.
     */
    public static int getFirstStrongCharacterDirection(String string) {
        assert string != null;
        return LocalizationUtilsJni.get().getFirstStrongCharacterDirection(string);
    }

    public static String getNativeUiLocale() {
        return LocalizationUtilsJni.get().getNativeUiLocale();
    }

    public static String substituteLocalePlaceholder(String str) {
        return str.replace("$LOCALE", LocaleUtils.getDefaultLocaleString().replace('-', '_'));
    }

    /**
     * Return the asset split language associated with a given Chromium language.
     *
     * This matches the directory used to store language-based assets in bundle APK splits.
     * E.g. for Hebrew, known as 'he' by Chromium, this method should return 'iw' because
     * the .pak file will be stored as /assets/locales#lang_iw/he.pak within the split.
     *
     * @param language Chromium specific language name.
     * @return Matching Android specific language name.
     */
    public static String getSplitLanguageForAndroid(String language) {
        // IMPORTANT: Keep in sync with the mapping found in:
        // build/android/gyp/util/resource_utils.py
        switch (language) {
            case "he":
                return "iw"; // Hebrew
            case "yi":
                return "ji"; // Yiddish
            case "id":
                return "in"; // Indonesian
            case "fil":
                return "tl"; // Filipino
            default:
                return language;
        }
    }

    /**
     * Return true iff a locale string matches a specific language string.
     *
     * @param locale Chromium locale name (e.g. "fil", or "en-US").
     * @param lang Chromium language name (e.g. "fi", or "en").
     * @return true iff the locale name matches the languages. E.g. should
     *         be false for ("fil", "fi") (Filipino locale + Finish language)
     *         but true for ("en-US", "en") (USA locale + English language).
     */
    public static boolean chromiumLocaleMatchesLanguage(String locale, String lang) {
        return LocaleUtils.toBaseLanguage(locale).equals(lang);
    }

    @NativeMethods
    interface Natives {
        int getFirstStrongCharacterDirection(String string);

        String getNativeUiLocale();
    }
}
