// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

/**
 * Represents a pointer to a C++ object borrowed by Java for an extended period.
 *
 * <p>Responsibility: Java holds a reference but does not own the object. The C++ lifecycle is
 * managed elsewhere. Java must call {@link #release()} when finished, which clears the reference
 * and decrements the BackupRefPtr quarantine reference count.
 *
 * <p>Not thread-safe: concurrent calls to {@link #release()}, or native calls racing with it, may
 * cause use-after-free or double-free.
 */
public interface JniRawPtr<T extends JniTypeToken> extends JniPtr<T> {
    /**
     * Releases the borrowed reference to the C++ object.
     *
     * <p>Calling {@code release()} again is a no-op. Any subsequent native call through this
     * instance will throw {@link IllegalStateException}.
     *
     * <p>Not thread-safe: concurrent calls to this method, or a native call racing with it, may
     * decrement the BackupRefPtr ref-count more than once and let the memory slot be reused while
     * another thread still holds the pointer.
     */
    void release();

    /** Factory method for creating mock/test instances with a fake pointer. */
    static <T extends JniTypeToken> JniRawPtr<T> createForTesting(long fakePtr) {
        return new JniRawPtrImpl<>(fakePtr);
    }
}
