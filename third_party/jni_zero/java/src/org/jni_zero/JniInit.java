// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

import java.util.Collections;

/** Used by jni_zero.cc. */
@JNINamespace("jni_zero")
public class JniInit {
    @CalledByNative
    private static Object[] init() {
        // For JVM (works fine on ART), cannot call from Java -> Native during InitVM because the
        // System.loadLibrary() call has not yet completed. Could work around this by using
        // RegisterNatives(), but simpler to return an array than to make Java->Native work.
        return new Object[] {Collections.EMPTY_LIST, Collections.EMPTY_MAP};
    }
}
