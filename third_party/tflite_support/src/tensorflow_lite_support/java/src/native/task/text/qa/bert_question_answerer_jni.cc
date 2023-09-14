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
#include "tensorflow_lite_support/cc/task/text/bert_question_answerer.h"
#include "tensorflow_lite_support/cc/utils/jni_utils.h"

namespace {

using ::tflite::support::StatusOr;
using ::tflite::support::utils::ConvertVectorToArrayList;
using ::tflite::support::utils::GetExceptionClassNameForStatusCode;
using ::tflite::support::utils::GetMappedFileBuffer;
using ::tflite::support::utils::JStringToString;
using ::tflite::support::utils::ThrowException;
using ::tflite::task::core::BaseOptions;
using ::tflite::task::text::BertQuestionAnswerer;
using ::tflite::task::text::BertQuestionAnswererOptions;
using ::tflite::task::text::QaAnswer;
using ::tflite::task::text::QuestionAnswerer;

constexpr int kInvalidPointer = 0;

// Creates a BertQuestionAnswererOptions proto based on the Java class.
BertQuestionAnswererOptions ConvertToProtoOptions(jlong base_options_handle) {
  BertQuestionAnswererOptions proto_options;

  if (base_options_handle != kInvalidPointer) {
    // proto_options will free the previous base_options and set the new one.
    proto_options.set_allocated_base_options(
        reinterpret_cast<BaseOptions*>(base_options_handle));
  }

  return proto_options;
}

}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_org_tensorflow_lite_task_text_qa_BertQuestionAnswerer_deinitJni(
    JNIEnv* env, jobject thiz, jlong native_handle) {
  delete reinterpret_cast<QuestionAnswerer*>(native_handle);
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_text_qa_BertQuestionAnswerer_initJniWithFileDescriptor(
    JNIEnv* env, jclass thiz, jint file_descriptor,
    jlong file_descriptor_length, jlong file_descriptor_offset,
    jlong base_options_handle) {
  BertQuestionAnswererOptions proto_options =
      ConvertToProtoOptions(base_options_handle);
  auto file_descriptor_meta = proto_options.mutable_base_options()
                                  ->mutable_model_file()
                                  ->mutable_file_descriptor_meta();
  file_descriptor_meta->set_fd(file_descriptor);
  if (file_descriptor_length > 0) {
    file_descriptor_meta->set_length(file_descriptor_length);
  }
  if (file_descriptor_offset > 0) {
    file_descriptor_meta->set_offset(file_descriptor_offset);
  }

  StatusOr<std::unique_ptr<QuestionAnswerer>> qa_status =
      BertQuestionAnswerer::CreateFromOptions(proto_options);
  if (qa_status.ok()) {
    return reinterpret_cast<jlong>(qa_status->release());
  } else {
    ThrowException(
        env, GetExceptionClassNameForStatusCode(qa_status.status().code()),
        "Error occurred when initializing BertQuestionAnswerer: %s",
        qa_status.status().message().data());
    return kInvalidPointer;
  }
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_text_qa_BertQuestionAnswerer_initJniWithBertByteBuffers(
    JNIEnv* env, jclass thiz, jobjectArray model_buffers) {
  absl::string_view model =
      GetMappedFileBuffer(env, env->GetObjectArrayElement(model_buffers, 0));
  absl::string_view vocab =
      GetMappedFileBuffer(env, env->GetObjectArrayElement(model_buffers, 1));

  StatusOr<std::unique_ptr<QuestionAnswerer>> qa_status =
      BertQuestionAnswerer::CreateBertQuestionAnswererFromBuffer(
          model.data(), model.size(), vocab.data(), vocab.size());
  if (qa_status.ok()) {
    return reinterpret_cast<jlong>(qa_status->release());
  } else {
    ThrowException(
        env, GetExceptionClassNameForStatusCode(qa_status.status().code()),
        "Error occurred when initializing BertQuestionAnswerer: %s",
        qa_status.status().message().data());
    return kInvalidPointer;
  }
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_text_qa_BertQuestionAnswerer_initJniWithAlbertByteBuffers(
    JNIEnv* env, jclass thiz, jobjectArray model_buffers) {
  absl::string_view model =
      GetMappedFileBuffer(env, env->GetObjectArrayElement(model_buffers, 0));
  absl::string_view sp_model =
      GetMappedFileBuffer(env, env->GetObjectArrayElement(model_buffers, 1));

  StatusOr<std::unique_ptr<QuestionAnswerer>> qa_status =
      BertQuestionAnswerer::CreateAlbertQuestionAnswererFromBuffer(
          model.data(), model.size(), sp_model.data(), sp_model.size());
  if (qa_status.ok()) {
    return reinterpret_cast<jlong>(qa_status->release());
  } else {
    ThrowException(
        env, GetExceptionClassNameForStatusCode(qa_status.status().code()),
        "Error occurred when initializing BertQuestionAnswerer: %s",
        qa_status.status().message().data());
    return kInvalidPointer;
  }
}

extern "C" JNIEXPORT jobject JNICALL
Java_org_tensorflow_lite_task_text_qa_BertQuestionAnswerer_answerNative(
    JNIEnv* env, jclass thiz, jlong native_handle, jstring context,
    jstring question) {
  auto* question_answerer = reinterpret_cast<QuestionAnswerer*>(native_handle);

  std::vector<QaAnswer> results = question_answerer->Answer(
      JStringToString(env, context), JStringToString(env, question));
  jclass qa_answer_class =
      env->FindClass("org/tensorflow/lite/task/text/qa/QaAnswer");
  jmethodID qa_answer_ctor =
      env->GetMethodID(qa_answer_class, "<init>", "(Ljava/lang/String;IIF)V");

  return ConvertVectorToArrayList(
      env, results.begin(), results.end(),
      [env, qa_answer_class, qa_answer_ctor](const QaAnswer& ans) {
        jstring text = env->NewStringUTF(ans.text.data());
        jobject qa_answer =
            env->NewObject(qa_answer_class, qa_answer_ctor, text, ans.pos.start,
                           ans.pos.end, ans.pos.logit);
        env->DeleteLocalRef(text);
        return qa_answer;
      });
}
