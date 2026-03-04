// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

/**
 * An interface for obtaining the raw pointer value. This is intended to be used internally by
 * generated JNI code and the JNI bridge itself, not by typical users.
 */
interface JniPtrInner<T extends JniTypeToken> extends JniPtr<T> {
    long getNativePtr();
}
