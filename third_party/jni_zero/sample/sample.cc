// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Generated file for JNI bindings from C++ to Java @CalledByNative methods.
// Only to be included in one .cc file.
// Name is based on the java file name: *.java -> jni/*_jni.h
namespace jni_zero::sample {
enum class MyEnum { A, B, C };
}

#include "third_party/jni_zero/sample/sample_jni/Sample_jni.h"

using jni_zero::JavaRef;
using jni_zero::ScopedJavaLocalRef;

namespace jni_zero::sample {

static void JNI_Sample_DoSomething(JNIEnv* env) {
  std::vector<MyEnum> values = Java_Sample_getArrayOfEnum(env);
  Java_Sample_setArrayOfEnum(env, values);
}

static jboolean JNI_Sample_TestMultipleParams(JNIEnv* env,
                                              jint a,
                                              jint b,
                                              const JavaRef<jstring>& c,
                                              const JavaRef<jobject>& d) {
  return jboolean(true);
}

static void JNI_Sample_CallBackIntoJava(JNIEnv* env) {
  jni_zero::sample::Java_Sample_staticCallback(env);
}

static ScopedJavaLocalRef<jobject> JNI_Sample_CallBackIntoInstance(
    JNIEnv* env,
    const JavaRef<jobject>& sample) {
  jni_zero::sample::Java_Sample_callback(env, sample);
  return ScopedJavaLocalRef<jobject>(sample);
}

}  // namespace jni_zero::sample

DEFINE_JNI(Sample)
