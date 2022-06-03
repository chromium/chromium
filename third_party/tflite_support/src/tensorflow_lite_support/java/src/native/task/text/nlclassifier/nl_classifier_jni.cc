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

#include "tensorflow/lite/kernels/kernel_util.h"
#include "tensorflow/lite/op_resolver.h"
#include "tensorflow_lite_support/cc/task/text/nlclassifier/nl_classifier.h"
#include "tensorflow_lite_support/cc/utils/jni_utils.h"
#include "tensorflow_lite_support/java/src/native/task/text/nlclassifier/nl_classifier_jni_utils.h"

namespace tflite {
namespace task {
// To be provided by a link-time library
extern std::unique_ptr<OpResolver> CreateOpResolver();

}  // namespace task
}  // namespace tflite

namespace {

using ::tflite::support::utils::GetMappedFileBuffer;
using ::tflite::support::utils::JStringToString;
using ::tflite::support::utils::kAssertionError;
using ::tflite::support::utils::kInvalidPointer;
using ::tflite::support::utils::ThrowException;
using ::tflite::task::text::nlclassifier::NLClassifier;
using ::tflite::task::text::nlclassifier::NLClassifierOptions;
using ::tflite::task::text::nlclassifier::RunClassifier;

NLClassifierOptions ConvertJavaNLClassifierOptions(
    JNIEnv* env,
    jobject java_nl_classifier_options) {
  jclass nl_classifier_options_class = env->FindClass(
      "org/tensorflow/lite/task/text/nlclassifier/"
      "NLClassifier$NLClassifierOptions");
  jmethodID input_tensor_index_method_id =
      env->GetMethodID(nl_classifier_options_class, "inputTensorIndex", "()I");
  jmethodID output_score_tensor_index_method_id = env->GetMethodID(
      nl_classifier_options_class, "outputScoreTensorIndex", "()I");
  jmethodID output_label_tensor_index_method_id = env->GetMethodID(
      nl_classifier_options_class, "outputLabelTensorIndex", "()I");
  jmethodID input_tensor_name_method_id = env->GetMethodID(
      nl_classifier_options_class, "inputTensorName", "()Ljava/lang/String;");
  jmethodID output_score_tensor_name_method_id =
      env->GetMethodID(nl_classifier_options_class, "outputScoreTensorName",
                       "()Ljava/lang/String;");
  jmethodID output_label_tensor_name_method_id =
      env->GetMethodID(nl_classifier_options_class, "outputLabelTensorName",
                       "()Ljava/lang/String;");

  return {
      .input_tensor_index = env->CallIntMethod(java_nl_classifier_options,
                                               input_tensor_index_method_id),
      .output_score_tensor_index = env->CallIntMethod(
          java_nl_classifier_options, output_score_tensor_index_method_id),
      .output_label_tensor_index = env->CallIntMethod(
          java_nl_classifier_options, output_label_tensor_index_method_id),
      .input_tensor_name = JStringToString(
          env, (jstring)env->CallObjectMethod(java_nl_classifier_options,
                                              input_tensor_name_method_id)),
      .output_score_tensor_name = JStringToString(
          env,
          (jstring)env->CallObjectMethod(java_nl_classifier_options,
                                         output_score_tensor_name_method_id)),
      .output_label_tensor_name = JStringToString(
          env,
          (jstring)env->CallObjectMethod(java_nl_classifier_options,
                                         output_label_tensor_name_method_id)),
  };
}

extern "C" JNIEXPORT void JNICALL
Java_org_tensorflow_lite_task_text_nlclassifier_NLClassifier_deinitJni(
    JNIEnv* env,
    jobject thiz,
    jlong native_handle) {
  delete reinterpret_cast<NLClassifier*>(native_handle);
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_text_nlclassifier_NLClassifier_initJniWithByteBuffer(
    JNIEnv* env,
    jclass thiz,
    jobject nl_classifier_options,
    jobject model_buffer) {
  auto model = GetMappedFileBuffer(env, model_buffer);
  tflite::support::StatusOr<std::unique_ptr<NLClassifier>> status =
      NLClassifier::CreateFromBufferAndOptions(
          model.data(), model.size(),
          ConvertJavaNLClassifierOptions(env, nl_classifier_options),
          tflite::task::CreateOpResolver());

  if (status.ok()) {
    return reinterpret_cast<jlong>(status->release());
  } else {
    ThrowException(env, kAssertionError,
                   "Error occurred when initializing NLClassifier: %s",
                   status.status().message().data());
    return kInvalidPointer;
  }
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_text_nlclassifier_NLClassifier_initJniWithFileDescriptor(
    JNIEnv* env,
    jclass thiz,
    jobject nl_classifier_options,
    jint fd) {
  tflite::support::StatusOr<std::unique_ptr<NLClassifier>> status =
      NLClassifier::CreateFromFdAndOptions(
          fd, ConvertJavaNLClassifierOptions(env, nl_classifier_options),
          tflite::task::CreateOpResolver());
  if (status.ok()) {
    return reinterpret_cast<jlong>(status->release());
  } else {
    ThrowException(env, kAssertionError,
                   "Error occurred when initializing NLClassifier: %s",
                   status.status().message().data());
    return kInvalidPointer;
  }
}

extern "C" JNIEXPORT jobject JNICALL
Java_org_tensorflow_lite_task_text_nlclassifier_NLClassifier_classifyNative(
    JNIEnv* env,
    jclass thiz,
    jlong native_handle,
    jstring text) {
  return RunClassifier(env, native_handle, text);
}

}  // namespace
