// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/jni_zero/common_apis.h"

#include "third_party/jni_zero/jni_zero_jni/JniUtil_jni.h"
#include "third_party/jni_zero/system_jni/Arrays_jni.h"
#include "third_party/jni_zero/system_jni/Collection_jni.h"

namespace jni_zero {

ScopedJavaLocalRef<jobjectArray> CollectionToArray(
    JNIEnv* env,
    const JavaRef<jobject>& collection) {
  return JNI_Collection::Java_Collection_toArray(env, collection);
}

ScopedJavaLocalRef<jobject> ArrayToList(JNIEnv* env,
                                        const JavaRef<jobjectArray>& array) {
  return JNI_Arrays::Java_Arrays_asList(env, array);
}

ScopedJavaLocalRef<jobjectArray> MapToArray(JNIEnv* env,
                                            const JavaRef<jobject>& map) {
  return Java_JniUtil_mapToArray(env, map);
}
ScopedJavaLocalRef<jobject> ArrayToMap(JNIEnv* env,
                                       const JavaRef<jobjectArray>& array) {
  return Java_JniUtil_arrayToMap(env, array);
}

}  // namespace jni_zero
