// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * A helper class to determine if an exception is relevant to WebLayer. Called if an uncaught
 * exception is detected.
 */
@JNINamespace("weblayer")
public final class WebLayerExceptionFilter {
    // The filename prefix used by Chromium proguarding, which we use to
    // recognise stack frames that reference WebLayer.
    private static final String CHROMIUM_PREFIX = "chromium-";

    @CalledByNative
    private static boolean stackTraceContainsWebLayerCode(Throwable t) {
        for (StackTraceElement frame : t.getStackTrace()) {
            if (frame.getClassName().startsWith("org.chromium.")
                    || (frame.getFileName() != null
                            && frame.getFileName().startsWith(CHROMIUM_PREFIX))) {
                return true;
            }
        }
        return false;
    }
}
