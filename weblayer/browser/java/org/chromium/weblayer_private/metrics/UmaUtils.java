// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.metrics;

import android.os.SystemClock;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Utilities to support startup metrics
 */
@JNINamespace("weblayer")
public class UmaUtils {
    private static long sApplicationStartTimeMs;

    /**
     * Record the time in the application lifecycle at which WebLayer code first runs.
     */
    public static void recordMainEntryPointTime() {
        // We can't simply pass this down through a JNI call, since the JNI for weblayer
        // isn't initialized until we start the native content browser component, and we
        // then need the start time in the C++ side before we return to Java. As such we
        // save it in a static that the C++ can fetch once it has initialized the JNI.
        sApplicationStartTimeMs = SystemClock.uptimeMillis();
    }

    @CalledByNative
    public static long getApplicationStartTime() {
        return sApplicationStartTimeMs;
    }
}
