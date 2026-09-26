// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Generated file for JNI bindings from C++ to Java @CalledByNative methods.
// Only to be included in one .cc file.
// Name is based on the java file name: *.java -> jni/*_jni.h
namespace jni_zero::sample {
enum class MyEnum { A, B, C };
struct NativeObject;
}

#include <cstdint>

#include "third_party/jni_zero/sample/sample_jni/Sample_jni.h"

using jni_zero::JavaRef;
using jni_zero::ScopedJavaLocalRef;

namespace jni_zero::sample {

int32_t g_native_object_deleted_count = 0;

struct NativeObject {
  int32_t value;
  explicit NativeObject(int32_t v) : value(v) {}
  ~NativeObject() { g_native_object_deleted_count++; }
  int32_t GetValue(JNIEnv* env) const { return value; }
};

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

static void JNI_Sample_TriggerCallbackWithSafePtr(JNIEnv* env, int32_t value) {
  NativeObject obj(value);
  Java_Sample_acceptSafePtrFromCpp(env, &obj);

  static NativeObject* borrowed_obj = new NativeObject(0);
  borrowed_obj->value = value;
  Java_Sample_acceptRawPtrFromCpp(env, jni_zero::MakeRaw(borrowed_obj));

  Java_Sample_acceptUniquePtrFromCpp(env,
                                     jni_zero::MakeUnique<NativeObject>(value));
}

static jni_zero::JniUniquePtr<NativeObject> JNI_Sample_CreateNativeObject(
    JNIEnv* env,
    int32_t value) {
  return jni_zero::MakeUnique<NativeObject>(value);
}

static jni_zero::JniUniquePtr<NativeObject> JNI_Sample_CreateNullNativeObject(
    JNIEnv* env) {
  return nullptr;
}

static jni_zero::JniRawPtr<NativeObject> JNI_Sample_BorrowNativeObject(
    JNIEnv* env,
    int32_t value) {
  static NativeObject* obj = new NativeObject(0);
  obj->value = value;
  return jni_zero::MakeRaw(obj);
}

static bool JNI_Sample_IsNullPtr(JNIEnv* env, NativeObject* ptr) {
  return ptr == nullptr;
}

static int32_t JNI_Sample_ReadHeldPtrFromCpp(JNIEnv* env) {
  NativeObject* ptr = Java_Sample_getHeldPtrForCpp(env);
  return ptr ? ptr->value : -1;
}

static int32_t JNI_Sample_GetDeletedCount(JNIEnv* env) {
  return g_native_object_deleted_count;
}

}  // namespace jni_zero::sample

DEFINE_JNI(Sample)
