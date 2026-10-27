// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

/**
 * A basic implementation of JniPtr that holds a long. Used internally, often for short-term borrows
 * when returning raw pointers from native without taking ownership.
 */
class JniPtrImpl<T extends JniTypeToken> extends JniPtrImplBase<T> {
    @CalledByNative
    JniPtrImpl(long nativePtr) {
        super(nativePtr);
        if (nativePtr == 0) {
            throw new IllegalArgumentException(
                    "JNI Zero internal error: if the pointer is 0, it should be converted to null");
        }
    }

    // Invalidates the reference (does NOT delete the underlying object).
    // Called automatically by generated glue code.
    @CalledByNative
    void release() {
        mNativePtr = 0;
    }

    @Override
    public long getNativePtr() {
        if (mNativePtr == 0) {
            throw new IllegalStateException(
                    "Safe JNI Pointer violation: Attempted to access a JniPtr after it was"
                            + " released.");
        }
        return mNativePtr;
    }
}
