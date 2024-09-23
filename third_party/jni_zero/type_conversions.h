// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JNI_ZERO_TYPE_CONVERSIONS_H_
#define JNI_ZERO_TYPE_CONVERSIONS_H_

#include <jni.h>

#include "third_party/jni_zero/java_refs.h"

#if defined(__cpp_concepts) && __cpp_concepts >= 201907L
#define JNI_ZERO_ENABLE_TYPE_CONVERSIONS 1
#else
#define JNI_ZERO_ENABLE_TYPE_CONVERSIONS 0
#endif

namespace jni_zero {

#if JNI_ZERO_ENABLE_TYPE_CONVERSIONS
#define JNI_ZERO_CONVERSION_FAILED_MSG(name)                               \
  "Failed to find a " name                                                 \
  " specialization for the given type. Did you forget to include the "     \
  "header file that declares it?\n"                                        \
  "If this error originates from a generated _jni.h file, make sure that " \
  "the header that declares the specialization is #included before the "   \
  "_jni.h one."
#else
#define JNI_ZERO_CONVERSION_FAILED_MSG(x) "Use of @JniType requires C++20."
#endif

template <typename T>
inline T FromJniType(JNIEnv* env, const JavaRef<jobject>& obj) {
  static_assert(sizeof(T) == 0, JNI_ZERO_CONVERSION_FAILED_MSG("FromJniType"));
}

template <typename T>
inline ScopedJavaLocalRef<jobject> ToJniType(JNIEnv* env, const T& obj) {
  static_assert(sizeof(T) == 0, JNI_ZERO_CONVERSION_FAILED_MSG("ToJniType"));
}

// Allow conversions using pointers by wrapping non-pointer conversions.
// Cannot live in default_conversions.h because we want code to be able to
// specialize it.
template <typename T>
inline ScopedJavaLocalRef<jobject> ToJniType(JNIEnv* env, T* value) {
  if (!value) {
    return nullptr;
  }
  return ToJniType(env, *value);
}

#if JNI_ZERO_ENABLE_TYPE_CONVERSIONS
#undef JNI_ZERO_CONVERSION_FAILED_MSG
#define JNI_ZERO_CONVERSION_FAILED_MSG(name)                             \
  "Failed to find a " name                                               \
  " specialization for the given type.\n"                                \
  "If this error is from a generated _jni.h file, ensure that the type " \
  "conforms to the container concepts defined in "                       \
  "jni_zero/default_conversions.h.\n"                                    \
  "If this error is from a non-generated call, ensure that there "       \
  "exists an #include for jni_zero/default_conversions.h."
#endif

// Convert from an stl container to a Java array. Uses ToJniType() on each
// element.
template <typename T>
inline ScopedJavaLocalRef<jobjectArray> ToJniArray(JNIEnv* env,
                                                   const T& obj,
                                                   jclass array_class) {
  static_assert(sizeof(T) == 0, JNI_ZERO_CONVERSION_FAILED_MSG("ToJniArray"));
}

// Convert from a Java array to an stl container of primitive types.
template <typename T>
inline ScopedJavaLocalRef<jarray> ToJniArray(JNIEnv* env, const T& obj) {
  static_assert(sizeof(T) == 0, JNI_ZERO_CONVERSION_FAILED_MSG("ToJniArray"));
}

// Convert from a Java array to an stl container. Uses FromJniType() on each
// element for non-primitive types.
template <typename T>
inline T FromJniArray(JNIEnv* env, const JavaRef<jobject>& obj) {
  static_assert(sizeof(T) == 0, JNI_ZERO_CONVERSION_FAILED_MSG("FromJniArray"));
}

// Convert from an stl container to a Java List<> by using ToJniType() on each
// element.
template <typename T>
inline ScopedJavaLocalRef<jobject> ToJniList(JNIEnv* env, const T& obj) {
  static_assert(sizeof(T) == 0, JNI_ZERO_CONVERSION_FAILED_MSG("ToJniList"));
}

// Convert from a Java Collection<> to an stl container by using FromJniType()
// on each element.
template <typename T>
inline T FromJniCollection(JNIEnv* env, const JavaRef<jobject>& obj) {
  static_assert(sizeof(T) == 0,
                JNI_ZERO_CONVERSION_FAILED_MSG("FromJniCollection"));
}
#undef JNI_ZERO_CONVERSION_FAILED_MSG

}  // namespace jni_zero

#endif  // JNI_ZERO_TYPE_CONVERSIONS_H_
