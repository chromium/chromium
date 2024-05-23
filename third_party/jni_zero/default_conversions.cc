// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/jni_zero/default_conversions.h"

namespace jni_zero {
// Specialization for int64_t.
template <>
std::vector<int64_t> FromJniArray<std::vector<int64_t>>(
    JNIEnv* env,
    const JavaRef<jobject>& j_object) {
  jlongArray j_array = static_cast<jlongArray>(j_object.obj());
  jsize array_jsize = env->GetArrayLength(j_array);
  size_t array_size = static_cast<size_t>(array_jsize);
  std::vector<int64_t> ret;
  ret.resize(array_size);
  env->GetLongArrayRegion(j_array, 0, array_jsize, ret.data());
  return ret;
}

template <>
ScopedJavaLocalRef<jarray> ToJniArray<std::vector<int64_t>>(
    JNIEnv* env,
    const std::vector<int64_t>& vec) {
  jsize array_jsize = static_cast<jsize>(vec.size());
  jlongArray jia = env->NewLongArray(array_jsize);
  CheckException(env);
  env->SetLongArrayRegion(jia, 0, array_jsize, vec.data());
  return ScopedJavaLocalRef<jarray>(env, jia);
}

// Specialization for int32_t.
template <>
std::vector<int32_t> FromJniArray<std::vector<int32_t>>(
    JNIEnv* env,
    const JavaRef<jobject>& j_object) {
  jintArray j_array = static_cast<jintArray>(j_object.obj());
  jsize array_jsize = env->GetArrayLength(j_array);
  size_t array_size = static_cast<size_t>(array_jsize);
  std::vector<int32_t> ret;
  ret.resize(array_size);
  env->GetIntArrayRegion(j_array, 0, array_jsize, ret.data());
  return ret;
}

template <>
ScopedJavaLocalRef<jarray> ToJniArray<std::vector<int32_t>>(
    JNIEnv* env,
    const std::vector<int32_t>& vec) {
  jsize array_jsize = static_cast<jsize>(vec.size());
  jintArray jia = env->NewIntArray(array_jsize);
  CheckException(env);
  env->SetIntArrayRegion(jia, 0, array_jsize, vec.data());
  return ScopedJavaLocalRef<jarray>(env, jia);
}

// Specialization for byte array.
template <>
std::vector<uint8_t> FromJniArray<std::vector<uint8_t>>(
    JNIEnv* env,
    const JavaRef<jobject>& j_object) {
  jbyteArray j_array = static_cast<jbyteArray>(j_object.obj());
  jsize array_jsize = env->GetArrayLength(j_array);
  size_t array_size = static_cast<size_t>(array_jsize);
  std::vector<uint8_t> ret;
  ret.resize(array_size);
  env->GetByteArrayRegion(j_array, 0, array_jsize,
                          reinterpret_cast<jbyte*>(ret.data()));
  return ret;
}

template <>
ScopedJavaLocalRef<jarray> ToJniArray<std::vector<uint8_t>>(
    JNIEnv* env,
    const std::vector<uint8_t>& vec) {
  jsize array_jsize = static_cast<jsize>(vec.size());
  jbyteArray jia = env->NewByteArray(array_jsize);
  CheckException(env);
  env->SetByteArrayRegion(jia, 0, array_jsize,
                          reinterpret_cast<const jbyte*>(vec.data()));
  return ScopedJavaLocalRef<jarray>(env, jia);
}
}  // namespace jni_zero
