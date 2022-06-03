// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
 * Helper class to tell native code whether manual JNI registration is required.
 */
@JNINamespace("weblayer")
public final class WebViewCompatibilityHelperImpl {
    private static boolean sRequiresManualJniRegistration;

    @CalledByNative
    private static boolean requiresManualJniRegistration() {
        return sRequiresManualJniRegistration;
    }

    public static void setRequiresManualJniRegistration(boolean isRequired) {
        sRequiresManualJniRegistration = isRequired;
    }
}
