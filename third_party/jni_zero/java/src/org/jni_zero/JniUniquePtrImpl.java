// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

/**
 * Implementation of {@link JniUniquePtr} representing an owned C++ object.
 *
 * <p>Access to {@link #getNativePtr()} is provided via {@link JniPtrInner} for JNI Zero generated
 * glue code.
 */
class JniUniquePtrImpl<T extends JniTypeToken> implements JniUniquePtr<T>, JniPtrInner<T> {
    private final long mDeleter;
    private long mNativePointer;

    JniUniquePtrImpl(long nativePointer, long deleter) {
        assert nativePointer != 0;
        mNativePointer = nativePointer;
        mDeleter = deleter;
    }

    @Override
    public long getNativePtr() {
        if (mNativePointer == 0) {
            throw new IllegalStateException(
                    "Safe JNI Pointer violation: Attempted to access a JniUniquePtr after it was"
                            + " destroyed.");
        }
        return mNativePointer;
    }

    @Override
    public void destroy() {
        if (mNativePointer != 0) {
            long ptr = mNativePointer;
            mNativePointer = 0;
            if (mDeleter != 0) {
                CommonApisJni.get().deleteDeleterBasePtr(ptr, mDeleter);
            }
        }
    }
}
