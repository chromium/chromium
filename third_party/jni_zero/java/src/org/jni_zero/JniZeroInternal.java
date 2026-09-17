// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

/**
 * Internal helpers for JNI Zero generated code and C++ glue.
 *
 * <p>Application code must not call methods in this class directly.
 */
public final class JniZeroInternal {
    private JniZeroInternal() {}

    /**
     * Extracts the raw native pointer value from a safe pointer. Used by generated JNI glue code
     * and C++ entrypoints.
     */
    public static long getNativePtr(JniPtr<?> ptr) {
        return ptr == null ? 0 : ((JniPtrInner<?>) ptr).getNativePtr();
    }
}
