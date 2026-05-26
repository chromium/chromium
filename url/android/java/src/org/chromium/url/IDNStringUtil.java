// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.url;

import android.icu.text.IDNA;
import android.os.Build;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.net.IDN;
import java.util.StringJoiner;

/** This class is used to convert unicode IDN domain names to ASCII, when not building with ICU. */
@JNINamespace("url::android")
@NullMarked
public class IDNStringUtil {
    private static final @Nullable IDNA sIDNA;

    static {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N) {
            sIDNA = null;
        } else {
            sIDNA =
                    IDNA.getUTS46Instance(
                            IDNA.NONTRANSITIONAL_TO_ASCII | IDNA.NONTRANSITIONAL_TO_UNICODE);
            if (sIDNA == null) {
                throw new IllegalStateException("Failed to create IDNA instance");
            }
        }
    }

    /**
     * Attempts to convert a Unicode hostname to an ASCII hostname using IDN rules. Uses IDNA2008 on
     * Android API 24+, IDNA2003 otherwise. See also https://crbug.com/513446116. Additionally, the
     * resulting ASCII hostname is validated against RFC 1122 and RFC 1123 rules.
     */
    public static String idnToASCII(String unicodeHostname) {
        if (sIDNA == null) {
            return IDN.toASCII(unicodeHostname, IDN.USE_STD3_ASCII_RULES);
        }

        var asciiHostnameBuilder = new StringBuilder();
        var info = new IDNA.Info();
        sIDNA.nameToASCII(unicodeHostname, asciiHostnameBuilder, info);
        if (info.hasErrors()) {
            var errors = new StringJoiner(", ");
            for (var error : info.getErrors()) {
                errors.add(error.toString());
            }
            throw new IllegalArgumentException("Failed to convert IDN to ASCII: " + errors);
        }
        var asciiHostname = asciiHostnameBuilder.toString();

        // android.icu.text.IDNA does not check against RFC 1122 and RFC 1123, so we feed the
        // resulting ASCII hostname to java.net.IDN, which does offer this functionality. Validation
        // failures will throw exceptions; no need to use the result.
        IDN.toASCII(asciiHostname, IDN.USE_STD3_ASCII_RULES);

        return asciiHostname;
    }

    @CalledByNative
    private static @Nullable String nativeIdnToASCII(String src) {
        try {
            return idnToASCII(src);
        } catch (Exception e) {
            return null;
        }
    }
}
