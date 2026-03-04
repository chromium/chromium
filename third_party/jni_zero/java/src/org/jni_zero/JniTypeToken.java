// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero;

/**
 * A marker interface used as a type token to represent the underlying native C++ type of a pointer.
 * This is used to constrain type parameters of safe JNI pointer types.
 */
public interface JniTypeToken {}
