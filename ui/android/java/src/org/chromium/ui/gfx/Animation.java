// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.gfx;

import android.provider.Settings;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;

/**
 * Provides utility methods relating to system animation state on the current platform (i.e. Android
 * in this case). See ui/gfx/animation/animation_android.cc.
 */
@JNINamespace("gfx")
public class Animation {
    @CalledByNative
    private static boolean prefersReducedMotion() {
        // We default to assuming that animations are enabled, to avoid impacting the experience for
        // users that don't have ANIMATOR_DURATION_SCALE defined.
        final float defaultScale = 1f;
        float durationScale =
                Settings.Global.getFloat(
                        ContextUtils.getApplicationContext().getContentResolver(),
                        Settings.Global.ANIMATOR_DURATION_SCALE,
                        defaultScale);
        return durationScale == 0.0;
    }
}
