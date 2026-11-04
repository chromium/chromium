// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.native_theme;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.os.Build;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * JNI Bridge to propagate Android-side UI theme state and events to C++
 * ui::OsSettingsProviderAndroid.
 */
@JNINamespace("ui")
@NullMarked
public class OsSettingsProviderAndroidBridge {
    private static @Nullable ComponentCallbacks sSystemAccentColorObserver;

    /**
     * Set preferred color scheme on native side.
     *
     * @param isDark True for dark mode, false for light mode.
     */
    public static void setPreferredColorScheme(boolean isDark) {
        ThreadUtils.assertOnUiThread();
        OsSettingsProviderAndroidBridgeJni.get().setPreferredColorScheme(isDark);
    }

    /**
     * Starts observing the OS system accent color (API 31+) and pushes the initial value and
     * configuration-change updates to the native {@link OsSettingsProviderAndroid}.
     */
    public static void registerSystemAccentColorObserver() {
        ThreadUtils.assertOnUiThread();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S || sSystemAccentColorObserver != null) {
            return;
        }
        Context context = ContextUtils.getApplicationContext();
        updateSystemAccentColor(context);
        sSystemAccentColorObserver =
                new ComponentCallbacks() {
                    @Override
                    public void onConfigurationChanged(Configuration configuration) {
                        updateSystemAccentColor(context);
                    }

                    @Override
                    public void onLowMemory() {}
                };
        context.registerComponentCallbacks(sSystemAccentColorObserver);
    }

    private static void updateSystemAccentColor(Context context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
            return;
        }
        final int color;
        try {
            color = context.getColor(android.R.color.system_accent1_500);
        } catch (Resources.NotFoundException e) {
            return;
        }
        OsSettingsProviderAndroidBridgeJni.get().setAccentColor(color);
    }

    @NativeMethods
    interface Natives {
        void setPreferredColorScheme(boolean isDark);

        void setAccentColor(int color);
    }
}
