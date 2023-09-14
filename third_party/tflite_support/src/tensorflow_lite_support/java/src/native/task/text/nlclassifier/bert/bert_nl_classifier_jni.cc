/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include <jni.h>

#include "tensorflow_lite_support/cc/task/core/proto/base_options_proto_inc.h"
#include "tensorflow_lite_support/cc/task/text/bert_nl_classifier.h"
#include "tensorflow_lite_support/cc/task/text/proto/bert_nl_classifier_options_proto_inc.h"
#include "tensorflow_lite_support/cc/utils/jni_utils.h"
#include "tensorflow_lite_support/java/src/native/task/text/nlclassifier/nl_classifier_jni_utils.h"

namespace {

using ::tflite::support::utils::GetExceptionClassNameForStatusCode;
using ::tflite::support::utils::kInvalidPointer;
using ::tflite::support::utils::ThrowException;
using ::tflite::task::core::BaseOptions;
using ::tflite::task::text::BertNLClassifier;
using ::tflite::task::text::BertNLClassifierOptions;
using ::tflite::task::text::nlclassifier::RunClassifier;

BertNLClassifierOptions ConvertJavaBertNLClassifierOptions(
    JNIEnv* env, jobject java_options, jlong base_options_handle) {
  BertNLClassifierOptions proto_options;

  if (base_options_handle != kInvalidPointer) {
    // proto_options will free the previous base_options and set the new one.
    proto_options.set_allocated_base_options(
        reinterpret_cast<BaseOptions*>(base_options_handle));
  }
  return proto_options;
}

}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_org_tensorflow_lite_task_text_nlclassifier_BertNLClassifier_deinitJni(
    JNIEnv* env, jobject thiz, jlong native_handle) {
  delete reinterpret_cast<BertNLClassifier*>(native_handle);
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_text_nlclassifier_BertNLClassifier_initJniWithByteBuffer(
    JNIEnv* env, jclass thiz, jobject model_buffer, jobject java_options,
    jlong base_options_handle) {
  BertNLClassifierOptions proto_options = ConvertJavaBertNLClassifierOptions(
      env, java_options, base_options_handle);
  proto_options.mutable_base_options()->mutable_model_file()->set_file_content(
      static_cast<char*>(env->GetDirectBufferAddress(model_buffer)),
      static_cast<size_t>(env->GetDirectBufferCapacity(model_buffer)));

  tflite::support::StatusOr<std::unique_ptr<BertNLClassifier>> classifier_or =
      BertNLClassifier::CreateFromOptions(proto_options);
  if (classifier_or.ok()) {
    return reinterpret_cast<jlong>(classifier_or->release());
  } else {
    ThrowException(
        env, GetExceptionClassNameForStatusCode(classifier_or.status().code()),
        "Error occurred when initializing Bert NLClassifier: %s",
        classifier_or.status().message().data());
    return kInvalidPointer;
  }
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_text_nlclassifier_BertNLClassifier_initJniWithFileDescriptor(
    JNIEnv* env, jclass thiz, jint fd, jobject java_options,
    jlong base_options_handle) {
  BertNLClassifierOptions proto_options = ConvertJavaBertNLClassifierOptions(
      env, java_options, base_options_handle);
  proto_options.mutable_base_options()
      ->mutable_model_file()
      ->mutable_file_descriptor_meta()
      ->set_fd(fd);

  tflite::support::StatusOr<std::unique_ptr<BertNLClassifier>> classifier_or =
      BertNLClassifier::CreateFromOptions(proto_options);
  if (classifier_or.ok()) {
    return reinterpret_cast<jlong>(classifier_or->release());
  } else {
    ThrowException(
        env, GetExceptionClassNameForStatusCode(classifier_or.status().code()),
        "Error occurred when initializing Bert NLClassifier: %s",
        classifier_or.status().message().data());
    return kInvalidPointer;
  }
}

extern "C" JNIEXPORT jobject JNICALL
Java_org_tensorflow_lite_task_text_nlclassifier_BertNLClassifier_classifyNative(
    JNIEnv* env, jclass clazz, jlong native_handle, jstring text) {
  return RunClassifier(env, native_handle, text);
}
