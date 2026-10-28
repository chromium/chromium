// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

import org.jni_zero.internal.Nullable;

/**
 * Base class for {@link JniPtr} implementations holding a raw native pointer value.
 *
 * <p>{@link #equals(Object)} and {@link #hashCode()} reflect the current {@code mNativePtr} value,
 * which transitions to {@code 0} when {@code release()} or {@code destroy()} is called. Instances
 * stored in hash-based collections must be removed prior to invalidation.
 *
 * <p>External packages and generated code must access native pointers via {@link
 * JniZeroInternal#getNativePtr(JniPtr)}.
 */
abstract class JniPtrImplBase<T extends JniTypeToken> implements JniPtr<T> {
    interface Tracker {
        void destroy();
    }

    interface TrackerFactory {
        @Nullable
        Tracker create(Object target, @Nullable String creationContext);
    }

    interface RawPtrReleaser {
        void release(long ptr);
    }

    static @Nullable TrackerFactory sTrackerFactory;
    static @Nullable RawPtrReleaser sRawPtrReleaser;

    long mNativePtr;

    JniPtrImplBase(long nativePtr) {
        mNativePtr = nativePtr;
    }

    abstract long getNativePtr();

    @Override
    public boolean equals(@Nullable Object obj) {
        return obj instanceof JniPtrImplBase<?> other && mNativePtr == other.mNativePtr;
    }

    @Override
    public int hashCode() {
        return Long.hashCode(mNativePtr);
    }
}
