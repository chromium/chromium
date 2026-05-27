// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.color;

import android.content.Context;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * Factory class to allow C++ ui/color to fetch resolved colors from the registered Java
 * implementation in chrome/android.
 */
@NullMarked
@JNINamespace("ui")
public class ColorProviderBridgeFactory {
    @Nullable private static ColorProviderBridge sInstance;

    public static void setInstance(ColorProviderBridge instance) {
        sInstance = instance;
    }

    @CalledByNative
    public static long[] getThemeColors(Context context) {
        if (sInstance == null) {
            return new long[0];
        }
        return sInstance.getThemeColors(context);
    }
}
