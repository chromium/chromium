// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/jni_zero/jni_zero.h"
#ifdef JNI_ZERO_ENABLE_TYPE_CONVERSIONS
#include "third_party/jni_zero/default_conversions.h"

namespace jni_zero {

#define PRIMITIVE_ARRAY_CONVERSIONS(T, JTYPE, J)                          \
  template <>                                                             \
  std::vector<T> FromJniArray<std::vector<T>>(                            \
      JNIEnv * env, const JavaRef<jobject>& j_object) {                   \
    JTYPE##Array j_array = static_cast<JTYPE##Array>(j_object.obj());     \
    jsize array_jsize = env->GetArrayLength(j_array);                     \
    size_t array_size = static_cast<size_t>(array_jsize);                 \
    std::vector<T> ret;                                                   \
    ret.resize(array_size);                                               \
    env->Get##J##ArrayRegion(j_array, 0, array_jsize,                     \
                             reinterpret_cast<JTYPE*>(ret.data()));       \
    return ret;                                                           \
  }                                                                       \
  template <>                                                             \
  ScopedJavaLocalRef<jarray> ToJniArray<std::vector<T>>(                  \
      JNIEnv * env, const std::vector<T>& vec) {                          \
    jsize array_jsize = static_cast<jsize>(vec.size());                   \
    JTYPE##Array arr = env->New##J##Array(array_jsize);                   \
    CheckException(env);                                                  \
    env->Set##J##ArrayRegion(arr, 0, array_jsize,                         \
                             reinterpret_cast<const JTYPE*>(vec.data())); \
    return ScopedJavaLocalRef<jarray>(env, arr);                          \
  }

PRIMITIVE_ARRAY_CONVERSIONS(int64_t, jlong, Long)
PRIMITIVE_ARRAY_CONVERSIONS(int32_t, jint, Int)
PRIMITIVE_ARRAY_CONVERSIONS(int16_t, jshort, Short)
PRIMITIVE_ARRAY_CONVERSIONS(uint16_t, jchar, Char)
PRIMITIVE_ARRAY_CONVERSIONS(uint8_t, jbyte, Byte)
PRIMITIVE_ARRAY_CONVERSIONS(float, jfloat, Float)
PRIMITIVE_ARRAY_CONVERSIONS(double, jdouble, Double)

// Specialization for bool, because vector<bool> is a bitmask under-the-hood,
// not an actual vector of bool values, and thus can't be directly copied.
template <>
std::vector<bool> FromJniArray<std::vector<bool>>(
    JNIEnv* env,
    const JavaRef<jobject>& j_object) {
  jbooleanArray j_array = static_cast<jbooleanArray>(j_object.obj());
  jsize array_jsize = env->GetArrayLength(j_array);
  size_t array_size = static_cast<size_t>(array_jsize);
  auto arr = std::make_unique<jboolean[]>(array_size);
  env->GetBooleanArrayRegion(j_array, 0, array_jsize, arr.get());

  std::vector<bool> ret;
  ret.resize(array_size);
  for (size_t i = 0; i < array_size; ++i) {
    ret[i] = arr[i];
  }
  return ret;
}

template <>
ScopedJavaLocalRef<jarray> ToJniArray<std::vector<bool>>(
    JNIEnv* env,
    const std::vector<bool>& vec) {
  jsize array_jsize = static_cast<jsize>(vec.size());
  size_t array_size = static_cast<size_t>(array_jsize);

  auto arr = std::make_unique<jboolean[]>(array_size);
  for (size_t i = 0; i < array_size; ++i) {
    arr[i] = vec[i];
  }

  jbooleanArray j_array = env->NewBooleanArray(array_jsize);
  CheckException(env);
  env->SetBooleanArrayRegion(j_array, 0, array_jsize, arr.get());
  return ScopedJavaLocalRef<jarray>(env, j_array);
}
}  // namespace jni_zero
#endif  // JNI_ZERO_ENABLE_TYPE_CONVERSIONS
