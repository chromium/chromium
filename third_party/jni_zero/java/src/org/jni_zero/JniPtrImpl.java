// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

/**
 * A basic implementation of JniPtr that holds a long. Used internally, often for short-term borrows
 * when returning raw pointers from native without taking ownership.
 */
class JniPtrImpl<T extends JniTypeToken> implements JniPtrInner<T> {
    private long mNativePtr;

    JniPtrImpl(long nativePtr) {
        if (nativePtr == 0) {
            throw new IllegalArgumentException(
                    "JNI Zero internal error: if the pointer is 0, it should be converted to null");
        }
        mNativePtr = nativePtr;
    }

    // Invalidates the reference (does NOT delete the underlying object).
    // Called automatically by generated glue code.
    void release() {
        mNativePtr = 0;
    }

    @Override
    public long getNativePtr() {
        if (mNativePtr == 0) {
            throw new IllegalStateException("Trying to access an already-released JniPtr");
        }
        return mNativePtr;
    }
}
