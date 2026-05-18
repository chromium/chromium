// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.native_theme;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;

/**
 * JNI Bridge to propagate Android-side UI theme state and events to C++
 * ui::OsSettingsProviderAndroid.
 */
@JNINamespace("ui")
@NullMarked
public class OsSettingsProviderAndroidBridge {

    /**
     * Set preferred color scheme on native side.
     *
     * @param dark True for dark mode, false for light mode.
     */
    public static void setPreferredColorScheme(boolean isDark) {
        ThreadUtils.assertOnUiThread();
        OsSettingsProviderAndroidBridgeJni.get().setPreferredColorScheme(isDark);
    }

    @NativeMethods
    interface Natives {
        void setPreferredColorScheme(boolean isDark);
    }
}
