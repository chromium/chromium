// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

/**
 * A marker interface for safe JNI pointers.
 *
 * @param <T> The native type token corresponding to the C++ type this pointer points to.
 */
public interface JniPtr<T extends JniTypeToken> {}
