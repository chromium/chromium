// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _JNI_ZERO_STUB_CONVERSIONS_H_
#define _JNI_ZERO_STUB_CONVERSIONS_H_

#include "third_party/jni_zero/jni_zero.h"

namespace jni_zero {

// "conversion" functions, this file only has to compile not run.
template <>
tests::CPPClass FromJniType<tests::CPPClass>(JNIEnv* env,
                                             const JavaRef<jobject>& j_obj) {
  return {};
}
template <>
std::string FromJniType<std::string>(JNIEnv* env,
                                     const JavaRef<jobject>& input) {
  return {};
}
template <>
ScopedJavaLocalRef<jobject> ToJniType<std::string>(JNIEnv* env,
                                                   const std::string& input) {
  return {};
}
template <>
std::u16string FromJniType<std::u16string>(JNIEnv* env,
                                           const JavaRef<jobject>& input) {
  return {};
}
template <>
ScopedJavaLocalRef<jobject> ToJniType<std::u16string>(
    JNIEnv* env,
    const std::u16string& input) {
  return {};
}
template <>
ScopedJavaLocalRef<jobject> ToJniType<const char>(JNIEnv* env, const char* input) {
  return {};
}
template <>
tests::CPPClass* FromJniType<tests::CPPClass*>(JNIEnv* env,
                                               const JavaRef<jobject>& j_obj) {
  return nullptr;
}

}  // namespace jni_zero
#endif
