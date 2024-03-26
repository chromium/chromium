// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "third_party/jni_zero/test/sample_for_tests.h"

// Generated file for JNI bindings from C++ to Java @CalledByNative methods.
// Only to be included in one .cc file.
// Name is based on the java file name: *.java -> jni/*_jni.h
#include "third_party/jni_zero/test/test_jni/SampleForAnnotationProcessor_jni.h"
#include "third_party/jni_zero/test/test_jni/SampleForTests_jni.h"

using jni_zero::AttachCurrentThread;
using jni_zero::JavaParamRef;
using jni_zero::ScopedJavaLocalRef;

namespace jni_zero {

// "conversion" functions, this file only has to compile not run.
template <>
tests::CPPClass FromJniType<tests::CPPClass>(JNIEnv* env,
                                             const JavaRef<jobject>& j_obj) {
  return {};
}
template <>
std::string FromJniType<std::string>(JNIEnv* env,
                                     const JavaRef<jstring>& input) {
  return {};
}
template <>
ScopedJavaLocalRef<jstring> ToJniType<std::string>(JNIEnv* env,
                                                   const std::string& input) {
  return {};
}
template <>
std::u16string FromJniType<std::u16string>(JNIEnv* env,
                                           const JavaRef<jstring>& input) {
  return {};
}
template <>
ScopedJavaLocalRef<jstring> ToJniType<std::u16string>(
    JNIEnv* env,
    const std::u16string& input) {
  return {};
}
template <>
ScopedJavaLocalRef<jstring> ToJniType<const char*>(
    JNIEnv* env,
    const char * const& input) {
  return {};
}

// Specialized conversions for std::optional<std::basic_string<T>> since jstring
// is a nullable type but std::basic_string<T> is not.
template <>
std::optional<std::string> FromJniType<std::optional<std::string>>(
    JNIEnv* env,
    const JavaRef<jstring>& j_string) {
  if (!j_string) {
    return std::nullopt;
  }
  return std::optional<std::string>(FromJniType<std::string>(env, j_string));
}

template <>
std::optional<std::u16string> FromJniType<std::optional<std::u16string>>(
    JNIEnv* env,
    const JavaRef<jstring>& j_string) {
  if (!j_string) {
    return std::nullopt;
  }
  return std::optional<std::u16string>(
      FromJniType<std::u16string>(env, j_string));
}

namespace tests {

jdouble CPPClass::InnerClass::MethodOtherP0(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller) {
  return 0.0;
}

CPPClass::CPPClass() = default;

CPPClass::~CPPClass() = default;

// static
void CPPClass::Destroy(JNIEnv* env, const JavaParamRef<jobject>& caller) {
  delete this;
}

jint CPPClass::Method(JNIEnv* env,
                      const JavaParamRef<jobject>& caller,
                      std::vector<std::string>& strArray) {
  return 0;
}

void CPPClass::AddStructB(JNIEnv* env,
                          const JavaParamRef<jobject>& caller,
                          const JavaParamRef<jobject>& structb) {
  long key = Java_InnerStructB_getKey(env, structb);
  std::string value = jni_zero::FromJniType<std::string>(
      env, Java_InnerStructB_getValue(env, structb));
  map_[key] = value;
}

void CPPClass::IterateAndDoSomethingWithStructB(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller) {
  // Iterate over the elements and do something with them.
  for (std::map<long, std::string>::const_iterator it = map_.begin();
       it != map_.end(); ++it) {
    long key = it->first;
    std::string value = it->second;
    std::cout << key << value;
  }
  map_.clear();
}

ScopedJavaLocalRef<jstring> CPPClass::ReturnAString(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller) {
  return ScopedJavaLocalRef<jstring>();
}

// Static free functions declared and called directly from java.
static jlong JNI_SampleForTests_Init(JNIEnv* env,
                                     const JavaParamRef<jobject>& caller,
                                     const JavaParamRef<jstring>& param,
                                     jni_zero::ByteArrayView& bytes) {
  return static_cast<jlong>(bytes.length());
}

static jdouble JNI_SampleForTests_GetDoubleFunction(
    JNIEnv*,
    const JavaParamRef<jobject>&) {
  return 0;
}

static jfloat JNI_SampleForTests_GetFloatFunction(JNIEnv*) {
  return 0;
}

static void JNI_SampleForTests_SetNonPODDatatype(JNIEnv*,
                                                 const JavaParamRef<jobject>&,
                                                 const JavaParamRef<jobject>&) {
}

static ScopedJavaLocalRef<jobject> JNI_SampleForTests_GetNonPODDatatype(
    JNIEnv*,
    const JavaParamRef<jobject>&) {
  return ScopedJavaLocalRef<jobject>();
}

static ScopedJavaLocalRef<jstring> JNI_SampleForTests_GetNonPODDatatype(
    JNIEnv*,
    const JavaParamRef<jstring>&) {
  return ScopedJavaLocalRef<jstring>();
}

static ScopedJavaLocalRef<jobjectArray> JNI_SampleForTests_GetNonPODDatatype(
    JNIEnv*,
    const JavaParamRef<jobjectArray>&) {
  return ScopedJavaLocalRef<jobjectArray>();
}

static ScopedJavaLocalRef<jclass> JNI_SampleForTests_GetClass(
    JNIEnv* env,
    const JavaParamRef<jclass>& arg0) {
  return ScopedJavaLocalRef<jclass>();
}

static ScopedJavaLocalRef<jthrowable> JNI_SampleForTests_GetThrowable(
    JNIEnv* env,
    const JavaParamRef<jthrowable>& arg0) {
  return ScopedJavaLocalRef<jthrowable>();
}

}  // namespace tests
}  // namespace jni_zero

// Proxy natives.
static void JNI_SampleForAnnotationProcessor_Foo(JNIEnv* env) {}

static ScopedJavaLocalRef<jobject> JNI_SampleForAnnotationProcessor_Bar(
    JNIEnv* env,
    const JavaParamRef<jobject>& sample) {
  return jni_zero::tests::JNI_SampleForTests_GetNonPODDatatype(env, sample);
}

static ScopedJavaLocalRef<jstring> JNI_SampleForAnnotationProcessor_RevString(
    JNIEnv* env,
    const JavaParamRef<jstring>& stringToReverse) {
  return jni_zero::tests::JNI_SampleForTests_GetNonPODDatatype(env,
                                                               stringToReverse);
}

static ScopedJavaLocalRef<jobjectArray>
JNI_SampleForAnnotationProcessor_SendToNative(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& strs) {
  return jni_zero::tests::JNI_SampleForTests_GetNonPODDatatype(env, strs);
}

static ScopedJavaLocalRef<jobjectArray>
JNI_SampleForAnnotationProcessor_SendSamplesToNative(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& strs) {
  return jni_zero::tests::JNI_SampleForTests_GetNonPODDatatype(env, strs);
}

static jboolean JNI_SampleForAnnotationProcessor_HasPhalange(JNIEnv* env) {
  return jboolean(true);
}

static std::vector<int> JNI_SampleForAnnotationProcessor_TestAllPrimitives(
    JNIEnv* env,
    int zint,
    std::vector<int>& ints,
    jlong zlong,
    const JavaParamRef<jlongArray>& longs,
    jshort zshort,
    const JavaParamRef<jshortArray>& shorts,
    int zchar,
    const JavaParamRef<jcharArray>& chars,
    jbyte zbyte,
    const JavaParamRef<jbyteArray>& bytes,
    jdouble zdouble,
    const JavaParamRef<jdoubleArray>& doubles,
    jfloat zfloat,
    const JavaParamRef<jfloatArray>& floats,
    jboolean zbool,
    const JavaParamRef<jbooleanArray>& bools) {
  return {};
}

static void JNI_SampleForAnnotationProcessor_TestSpecialTypes(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jobjectArray>& classes,
    const JavaParamRef<jthrowable>& throwable,
    const JavaParamRef<jobjectArray>& throwables,
    const JavaParamRef<jstring>& string,
    const JavaParamRef<jobjectArray>& strings,
    std::string& convertedString,
    std::vector<std::string>& convertedStrings,
    std::optional<std::string>& optionalString,
    const JavaParamRef<jobject>& tStruct,
    const JavaParamRef<jobjectArray>& structs,
    const JavaParamRef<jobject>& obj,
    jni_zero::tests::CPPClass& convertedObj,
    const JavaParamRef<jobjectArray>& objs,
    std::vector<jni_zero::tests::CPPClass>& convertedObjs) {}

static ScopedJavaLocalRef<jthrowable>
JNI_SampleForAnnotationProcessor_ReturnThrowable(JNIEnv* env) {
  return ScopedJavaLocalRef<jthrowable>();
}

static ScopedJavaLocalRef<jobjectArray>
JNI_SampleForAnnotationProcessor_ReturnThrowables(JNIEnv* env) {
  return ScopedJavaLocalRef<jobjectArray>();
}

static ScopedJavaLocalRef<jclass> JNI_SampleForAnnotationProcessor_ReturnClass(
    JNIEnv* env) {
  return ScopedJavaLocalRef<jclass>();
}

static ScopedJavaLocalRef<jobjectArray>
JNI_SampleForAnnotationProcessor_ReturnClasses(JNIEnv* env) {
  return ScopedJavaLocalRef<jobjectArray>();
}

static ScopedJavaLocalRef<jstring>
JNI_SampleForAnnotationProcessor_ReturnString(JNIEnv* env) {
  return {};
}

static ScopedJavaLocalRef<jobjectArray>
JNI_SampleForAnnotationProcessor_ReturnStrings(JNIEnv* env) {
  return {};
}

static std::string JNI_SampleForAnnotationProcessor_ReturnConvertedString(
    JNIEnv* env) {
  return {};
}

static std::vector<std::string>
JNI_SampleForAnnotationProcessor_ReturnConvertedStrings(JNIEnv* env) {
  return {};
}

static std::vector<jni_zero::ScopedJavaLocalRef<jobject>>
JNI_SampleForAnnotationProcessor_ReturnConvertedAppObjects(JNIEnv* env) {
  return {};
}

static std::vector<jint> JNI_SampleForAnnotationProcessor_ReturnConvertedInts(
    JNIEnv* env) {
  return {};
}

static ScopedJavaLocalRef<jobject>
JNI_SampleForAnnotationProcessor_ReturnStruct(JNIEnv* env) {
  return ScopedJavaLocalRef<jobject>();
}

static ScopedJavaLocalRef<jobjectArray>
JNI_SampleForAnnotationProcessor_ReturnStructs(JNIEnv* env) {
  return ScopedJavaLocalRef<jobjectArray>();
}

static ScopedJavaLocalRef<jobject>
JNI_SampleForAnnotationProcessor_ReturnObject(JNIEnv* env) {
  return ScopedJavaLocalRef<jobject>();
}

static ScopedJavaLocalRef<jobjectArray>
JNI_SampleForAnnotationProcessor_ReturnObjects(JNIEnv* env) {
  return ScopedJavaLocalRef<jobjectArray>();
}

int main() {
  // On a regular application, you'd call AttachCurrentThread(). This sample is
  // not yet linking with all the libraries.
  JNIEnv* env = /* AttachCurrentThread() */ nullptr;

  // This is how you call a java static method from C++.
  bool foo = jni_zero::tests::Java_SampleForTests_staticJavaMethod(env);

  // This is how you call a java method from C++. Note that you must have
  // obtained the jobject somehow.
  ScopedJavaLocalRef<jobject> my_java_object;
  int bar = jni_zero::tests::Java_SampleForTests_javaMethod(env, my_java_object,
                                                            1, 2);

  jni_zero::tests::Java_SampleForTests_methodWithGenericParams(
      env, my_java_object, nullptr, nullptr);

  // This is how you call a java constructor method from C++.
  ScopedJavaLocalRef<jobject> my_created_object =
      jni_zero::tests::Java_SampleForTests_Constructor(env, 1, 2);

  std::vector<const char*> string_vector = {"Test"};
  std::string first_string =
      jni_zero::tests::Java_SampleForTests_getFirstString(
          env, my_created_object, string_vector);

  jni_zero::tests::Java_SampleForTests_methodWithAnnotationParamAssignment(
      env, my_created_object);

  std::cout << foo << bar << first_string;

  for (int i = 0; i < 10; ++i) {
    // Creates a "struct" that will then be used by the java side.
    ScopedJavaLocalRef<jobject> struct_a =
        jni_zero::tests::Java_InnerStructA_create(
            env, 0, 1, ScopedJavaLocalRef<jstring>());
    jni_zero::tests::Java_SampleForTests_addStructA(env, my_java_object,
                                                    struct_a);
  }
  jni_zero::tests::Java_SampleForTests_iterateAndDoSomething(env,
                                                             my_java_object);
  jni_zero::tests::Java_SampleForTests_packagePrivateJavaMethod(env,
                                                                my_java_object);
  jni_zero::tests::Java_SampleForTests_methodThatThrowsException(
      env, my_java_object);
  jni_zero::tests::Java_SampleForTests_javaMethodWithAnnotatedParam(
      env, my_java_object, 42, 13, -1, 99);

  jni_zero::tests::Java_SampleForTests_getInnerInterface(env);
  jni_zero::tests::Java_SampleForTests_getInnerEnum(env);
  jni_zero::tests::Java_SampleForTests_getInnerEnum(env, 0);

  ScopedJavaLocalRef<jthrowable> throwable;
  throwable = jni_zero::tests::Java_SampleForTests_getThrowable(
      env, my_java_object, throwable);

  ScopedJavaLocalRef<jclass> clazz;
  clazz =
      jni_zero::tests::Java_SampleForTests_getClass(env, my_java_object, clazz);

  return 0;
}
