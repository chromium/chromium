// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

/**
 * An interface for obtaining the raw pointer value.
 *
 * <p>External packages and generated code must access native pointers via {@link
 * JniZeroInternal#getNativePtr(JniPtr)}.
 */
interface JniPtrInner<T extends JniTypeToken> extends JniPtr<T> {
    long getNativePtr();
}
