// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

/**
 * Internal implementation of {@link JniRawPtr}.
 *
 * <p>Enforces explicit lifecycle tracking and safe pointer invalidation.
 */
class JniRawPtrImpl<T extends JniTypeToken> implements JniRawPtr<T>, JniPtrInner<T> {
    private long mNativePointer;

    JniRawPtrImpl(long nativePointer) {
        assert nativePointer != 0;
        mNativePointer = nativePointer;
    }

    @Override
    public long getNativePtr() {
        if (mNativePointer == 0) {
            throw new IllegalStateException(
                    "Safe JNI Pointer violation: Attempted to use a JniRawPtr after it has"
                            + " already been released.");
        }
        return mNativePointer;
    }

    @Override
    public void release() {
        if (mNativePointer != 0) {
            long ptr = mNativePointer;
            mNativePointer = 0;
            if (JniZero.isRawPtrHooksEnabled()) {
                CommonApisJni.get().releaseRawPtr(ptr);
            }
        }
    }
}
