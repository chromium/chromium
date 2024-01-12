// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JNI_ZERO_CORE_H_
#define JNI_ZERO_CORE_H_

#include <jni.h>

#include <string>

#include "third_party/jni_zero/jni_export.h"

namespace jni_zero {
// Attaches the current thread to the VM (if necessary) and return the JNIEnv*.
JNI_ZERO_COMPONENT_BUILD_EXPORT JNIEnv* AttachCurrentThread();

// Same to AttachCurrentThread except that thread name will be set to
// |thread_name| if it is the first call. Otherwise, thread_name won't be
// changed. AttachCurrentThread() doesn't regard underlying platform thread
// name, but just resets it to "Thread-???". This function should be called
// right after new thread is created if it is important to keep thread name.
JNI_ZERO_COMPONENT_BUILD_EXPORT JNIEnv* AttachCurrentThreadWithName(
    const std::string& thread_name);

// Detaches the current thread from VM if it is attached.
JNI_ZERO_COMPONENT_BUILD_EXPORT void DetachFromVM();

// Initializes the global JVM.
JNI_ZERO_COMPONENT_BUILD_EXPORT void InitVM(JavaVM* vm);

// Returns true if the global JVM has been initialized.
JNI_ZERO_COMPONENT_BUILD_EXPORT bool IsVMInitialized();

// Returns the global JVM, or nullptr if it has not been initialized.
JNI_ZERO_COMPONENT_BUILD_EXPORT JavaVM* GetVM();

// Do not allow any future native->java calls.
// This is necessary in gtest DEATH_TESTS to prevent
// GetJavaStackTraceIfPresent() from accessing a defunct JVM (due to fork()).
// https://crbug.com/1484834
JNI_ZERO_COMPONENT_BUILD_EXPORT void DisableJvmForTesting();

JNI_ZERO_COMPONENT_BUILD_EXPORT void SetExceptionHandler(
    void (*callback)(JNIEnv*));

// Returns true if an exception is pending in the provided JNIEnv*.
JNI_ZERO_COMPONENT_BUILD_EXPORT bool HasException(JNIEnv* env);

// If an exception is pending in the provided JNIEnv*, this function clears it
// and returns true.
JNI_ZERO_COMPONENT_BUILD_EXPORT bool ClearException(JNIEnv* env);

// If there's any pending exception, this function will call the set exception
// handler, or if none are set, it will fatally LOG.
JNI_ZERO_COMPONENT_BUILD_EXPORT void CheckException(JNIEnv* env);

}  // namespace jni_zero

#endif  // JNI_ZERO_CORE_H_
