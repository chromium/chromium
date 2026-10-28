// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

import org.jni_zero.internal.Nullable;

/**
 * Internal implementation of {@link JniRawPtr}.
 *
 * <p>Enforces explicit lifecycle tracking and safe pointer invalidation.
 */
class JniRawPtrImpl<T extends JniTypeToken> extends JniPtrImplBase<T> implements JniRawPtr<T> {
    final @Nullable Tracker mTracker;

    JniRawPtrImpl(long nativePointer) {
        this(nativePointer, null);
    }

    @CalledByNative
    JniRawPtrImpl(long nativePointer, @Nullable String creationContext) {
        super(nativePointer);
        assert nativePointer != 0;
        TrackerFactory factory = sTrackerFactory;
        mTracker = factory == null ? null : factory.create(this, creationContext);
    }

    @Override
    public long getNativePtr() {
        if (mNativePtr == 0) {
            throw new IllegalStateException(
                    "Safe JNI Pointer violation: Attempted to use a JniRawPtr after it has"
                            + " already been released.");
        }
        return mNativePtr;
    }

    @Override
    public void release() {
        if (mNativePtr != 0) {
            if (mTracker != null) {
                mTracker.destroy();
            }
            long ptr = mNativePtr;
            mNativePtr = 0;
            RawPtrReleaser releaser = sRawPtrReleaser;
            if (releaser != null) {
                releaser.release(ptr);
            }
        }
    }
}
