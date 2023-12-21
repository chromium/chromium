// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JNI_ZERO_CORE_H_
#define JNI_ZERO_CORE_H_

#include <jni.h>

#include "third_party/jni_zero/jni_export.h"

namespace jni_zero {
// Attaches the current thread to the VM (if necessary) and return the JNIEnv*.
JNI_ZERO_COMPONENT_BUILD_EXPORT JNIEnv* AttachCurrentThread();

// Initializes the global JVM.
JNI_ZERO_COMPONENT_BUILD_EXPORT void InitVM(JavaVM* vm);

// Do not allow any future native->java calls.
// This is necessary in gtest DEATH_TESTS to prevent
// GetJavaStackTraceIfPresent() from accessing a defunct JVM (due to fork()).
// https://crbug.com/1484834
JNI_ZERO_COMPONENT_BUILD_EXPORT void DisableJvmForTesting();

JNI_ZERO_COMPONENT_BUILD_EXPORT void SetExceptionHandler(
    void (*callback)(JNIEnv*));

// If there's any pending exception, this function will call the set exception
// handler, or if none are set, it will fatally LOG.
JNI_ZERO_COMPONENT_BUILD_EXPORT void CheckException(JNIEnv* env);

}  // namespace jni_zero

#endif  // JNI_ZERO_CORE_H_
