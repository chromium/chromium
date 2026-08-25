// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/jni_zero/jni_zero.h"

#ifdef JNI_ZERO_ENABLE_TYPE_CONVERSIONS
#include <array>

#include "third_party/jni_zero/default_conversions.h"

namespace jni_zero {

#define PRIMITIVE_ARRAY_CONVERSIONS(T, JTYPE, J)                      \
  template <>                                                         \
  std::vector<T> FromJniArray<std::vector<T>>(                        \
      JNIEnv * env, const JavaRef<jobject>& j_object) {               \
    std::vector<T> ret;                                               \
    if (!j_object)                                                    \
      return ret;                                                     \
    JTYPE##Array j_array = static_cast<JTYPE##Array>(j_object.obj()); \
    jsize array_jsize = env->GetArrayLength(j_array);                 \
    size_t array_size = static_cast<size_t>(array_jsize);             \
    if (array_size == 0) {                                            \
      return ret;                                                     \
    }                                                                 \
    ret.resize(array_size);                                           \
    env->Get##J##ArrayRegion(j_array, 0, array_jsize,                 \
                             reinterpret_cast<JTYPE*>(ret.data()));   \
    return ret;                                                       \
  }                                                                   \
  template <>                                                         \
  ScopedJavaLocalRef<jarray> ToJniArray<std::vector<T>>(              \
      JNIEnv * env, const std::vector<T>& vec) {                      \
    return jni_zero::NewArray(env, vec);                              \
  }

PRIMITIVE_ARRAY_CONVERSIONS(int64_t, jlong, Long)
PRIMITIVE_ARRAY_CONVERSIONS(int32_t, jint, Int)
PRIMITIVE_ARRAY_CONVERSIONS(int16_t, jshort, Short)
PRIMITIVE_ARRAY_CONVERSIONS(uint16_t, jchar, Char)
PRIMITIVE_ARRAY_CONVERSIONS(int8_t, jbyte, Byte)
PRIMITIVE_ARRAY_CONVERSIONS(float, jfloat, Float)
PRIMITIVE_ARRAY_CONVERSIONS(double, jdouble, Double)
// Stamp out different-signed versions and rely on ICF to de-dupe them.
PRIMITIVE_ARRAY_CONVERSIONS(uint64_t, jlong, Long)
PRIMITIVE_ARRAY_CONVERSIONS(uint32_t, jint, Int)
PRIMITIVE_ARRAY_CONVERSIONS(char16_t, jchar, Char)
PRIMITIVE_ARRAY_CONVERSIONS(uint8_t, jbyte, Byte)
PRIMITIVE_ARRAY_CONVERSIONS(char, jbyte, Byte)

// Specialization for bool, because vector<bool> is a bitmask under-the-hood,
// not an actual vector of bool values, and thus can't be directly copied.
template <>
std::vector<bool> FromJniArray<std::vector<bool>>(
    JNIEnv* env,
    const JavaRef<jobject>& j_object) {
  jbooleanArray j_array = static_cast<jbooleanArray>(j_object.obj());
  jsize array_jsize = env->GetArrayLength(j_array);
  size_t array_size = static_cast<size_t>(array_jsize);
  std::vector<bool> ret;
  ret.resize(array_size);

  if (array_size <= 1024) {
    std::array<jboolean, 1024> arr;
    env->GetBooleanArrayRegion(j_array, 0, array_jsize, arr.data());
    for (size_t i = 0; i < array_size; ++i) {
      ret[i] = arr[i];
    }
  } else {
    std::vector<jboolean> arr(array_size);
    env->GetBooleanArrayRegion(j_array, 0, array_jsize, arr.data());
    for (size_t i = 0; i < array_size; ++i) {
      ret[i] = arr[i];
    }
  }
  return ret;
}

template <>
ScopedJavaLocalRef<jarray> ToJniArray<std::vector<bool>>(
    JNIEnv* env,
    const std::vector<bool>& vec) {
  return jni_zero::NewArray(env, vec);
}
}  // namespace jni_zero
#endif  // JNI_ZERO_ENABLE_TYPE_CONVERSIONS
