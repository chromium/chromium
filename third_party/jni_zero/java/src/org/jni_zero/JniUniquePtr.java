// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

/**
 * High-level wrapper for an owned C++ pointer passed across JNI.
 *
 * <p>Java takes full ownership of the native object and must explicitly destroy it using {@link
 * #destroy()}.
 *
 * @param <T> The native type token defining the C++ class.
 */
public interface JniUniquePtr<T extends JniTypeToken> extends JniPtr<T> {
    /**
     * Destroys the underlying C++ object using the bound C++ deleter.
     *
     * <p>Calling {@code destroy()} again is a no-op. Any subsequent native call through this
     * instance will throw {@link IllegalStateException}.
     *
     * <p>Not thread-safe: concurrent calls to this method, or a native call racing with it, may
     * cause a double-free or use-after-free, as with {@code std::unique_ptr}.
     */
    void destroy();

    /**
     * Factory method for creating mock/test instances with a fake pointer.
     *
     * <p>The returned unique pointer does not trigger native deletion on destroy.
     */
    static <T extends JniTypeToken> JniUniquePtr<T> createForTesting(long fakePtr) {
        return new JniUniquePtrImpl<>(fakePtr, 0);
    }
}
