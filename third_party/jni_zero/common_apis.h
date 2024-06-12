// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JNI_ZERO_COMMON_APIS_H_
#define JNI_ZERO_COMMON_APIS_H_

#include "third_party/jni_zero/jni_zero.h"

namespace jni_zero {
// Wraps Collection.toArray().
JNI_ZERO_COMPONENT_BUILD_EXPORT ScopedJavaLocalRef<jobjectArray>
CollectionToArray(JNIEnv* env, const JavaRef<jobject>& collection);
// Wraps Arrays.asList().
JNI_ZERO_COMPONENT_BUILD_EXPORT ScopedJavaLocalRef<jobject> ArrayToList(
    JNIEnv* env,
    const JavaRef<jobjectArray>& array);
// Serializes a Map to an array of key/values.
JNI_ZERO_COMPONENT_BUILD_EXPORT ScopedJavaLocalRef<jobjectArray> MapToArray(
    JNIEnv* env,
    const JavaRef<jobject>& map);
JNI_ZERO_COMPONENT_BUILD_EXPORT ScopedJavaLocalRef<jobject> ArrayToMap(
    JNIEnv* env,
    const JavaRef<jobjectArray>& array);
}  // namespace jni_zero

#endif  // JNI_ZERO_COMMON_APIS_H_
