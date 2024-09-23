// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Generated file for JNI bindings from C++ to Java @CalledByNative methods.
// Only to be included in one .cc file.
// Name is based on the java file name: *.java -> jni/*_jni.h
#include "third_party/jni_zero/sample/sample_header/Sample_jni.h"

using jni_zero::JavaParamRef;
using jni_zero::ScopedJavaLocalRef;

namespace jni_zero::sample {
static void JNI_Sample_DoNothing(JNIEnv* env) {}

static jboolean JNI_Sample_TestMultipleParams(JNIEnv* env,
                                              jint a,
                                              jint b,
                                              const JavaParamRef<jstring>& c,
                                              const JavaParamRef<jobject>& d) {
  return jboolean(true);
}

static void JNI_Sample_CallBackIntoJava(JNIEnv* env) {
  jni_zero::sample::Java_Sample_staticCallback(env);
}

static ScopedJavaLocalRef<jobject> JNI_Sample_CallBackIntoInstance(
    JNIEnv* env,
    const JavaParamRef<jobject>& sample) {
  jni_zero::sample::Java_Sample_callback(env, sample);
  return ScopedJavaLocalRef<jobject>(sample);
}

}  // namespace jni_zero::sample
