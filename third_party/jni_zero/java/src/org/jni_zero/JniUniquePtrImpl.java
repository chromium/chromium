// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

import org.jni_zero.internal.Nullable;

/**
 * Implementation of {@link JniUniquePtr} representing an owned C++ object.
 *
 * <p>Access to {@link #getNativePtr()} is provided via {@link JniPtrImplBase} for JNI Zero
 * generated glue code.
 */
class JniUniquePtrImpl<T extends JniTypeToken> extends JniPtrImplBase<T>
        implements JniUniquePtr<T> {
    private final long mDeleter;
    private final @Nullable Tracker mTracker;

    JniUniquePtrImpl(long nativePointer, long deleter) {
        this(nativePointer, deleter, null);
    }

    @CalledByNative
    JniUniquePtrImpl(long nativePointer, long deleter, @Nullable String creationContext) {
        super(nativePointer);
        assert nativePointer != 0;
        mDeleter = deleter;
        TrackerFactory factory = sTrackerFactory;
        mTracker = factory == null ? null : factory.create(this, creationContext);
    }

    @Override
    public long getNativePtr() {
        if (mNativePtr == 0) {
            throw new IllegalStateException(
                    "Safe JNI Pointer violation: Attempted to access a JniUniquePtr after it was"
                            + " destroyed.");
        }
        return mNativePtr;
    }

    @Override
    public void destroy() {
        if (mNativePtr != 0) {
            if (mTracker != null) {
                mTracker.destroy();
            }
            long ptr = mNativePtr;
            mNativePtr = 0;
            if (mDeleter != 0) {
                CommonApisJni.get().deleteDeleterBasePtr(ptr, mDeleter);
            }
        }
    }
}
