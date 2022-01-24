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

#include "tensorflow_lite_support/cc/task/text/nlclassifier/bert_nl_classifier.h"
#include "tensorflow_lite_support/cc/utils/jni_utils.h"
#include "tensorflow_lite_support/java/src/native/task/text/nlclassifier/nl_classifier_jni_utils.h"

namespace {

using ::tflite::support::utils::GetMappedFileBuffer;
using ::tflite::support::utils::kAssertionError;
using ::tflite::support::utils::kInvalidPointer;
using ::tflite::support::utils::ThrowException;
using ::tflite::task::text::nlclassifier::BertNLClassifier;
using ::tflite::task::text::nlclassifier::RunClassifier;

extern "C" JNIEXPORT void JNICALL
Java_org_tensorflow_lite_task_text_nlclassifier_BertNLClassifier_deinitJni(
    JNIEnv* env,
    jobject thiz,
    jlong native_handle) {
  delete reinterpret_cast<BertNLClassifier*>(native_handle);
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_text_nlclassifier_BertNLClassifier_initJniWithByteBuffer(
    JNIEnv* env,
    jclass thiz,
    jobject model_buffer) {
  auto model = GetMappedFileBuffer(env, model_buffer);
  tflite::support::StatusOr<std::unique_ptr<BertNLClassifier>> status =
      BertNLClassifier::CreateFromBuffer(model.data(), model.size());
  if (status.ok()) {
    return reinterpret_cast<jlong>(status->release());
  } else {
    ThrowException(env, kAssertionError,
                   "Error occurred when initializing Bert NLClassifier: %s",
                   status.status().message().data());
    return kInvalidPointer;
  }
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_text_nlclassifier_BertNLClassifier_initJniWithFileDescriptor(
    JNIEnv* env,
    jclass thiz,
    jint fd) {
  tflite::support::StatusOr<std::unique_ptr<BertNLClassifier>> status =
      BertNLClassifier::CreateFromFd(fd);
  if (status.ok()) {
    return reinterpret_cast<jlong>(status->release());
  } else {
    ThrowException(env, kAssertionError,
                   "Error occurred when initializing Bert NLClassifier: %s",
                   status.status().message().data());
    return kInvalidPointer;
  }
}

extern "C" JNIEXPORT jobject JNICALL
Java_org_tensorflow_lite_task_text_nlclassifier_BertNLClassifier_classifyNative(
    JNIEnv* env,
    jclass clazz,
    jlong native_handle,
    jstring text) {
  return RunClassifier(env, native_handle, text);
}

}  // namespace
