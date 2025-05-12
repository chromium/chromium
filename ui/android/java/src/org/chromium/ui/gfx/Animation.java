// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.gfx;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.accessibility.AccessibilityState;

/**
 * Provides utility methods relating to system animation state on the current platform (i.e. Android
 * in this case). See ui/gfx/animation/animation_android.cc.
 */
@JNINamespace("gfx")
@NullMarked
public class Animation {
    /** Returns whether the user settings specify preferred reduced motion. */
    @CalledByNative
    private static boolean prefersReducedMotion() {
        return AccessibilityState.prefersReducedMotion();
    }
}
