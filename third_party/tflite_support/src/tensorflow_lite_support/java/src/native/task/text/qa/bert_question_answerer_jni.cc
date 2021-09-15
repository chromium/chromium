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

#include "tensorflow_lite_support/cc/task/text/qa/bert_question_answerer.h"
#include "tensorflow_lite_support/cc/utils/jni_utils.h"

namespace {

using ::tflite::support::utils::ConvertVectorToArrayList;
using ::tflite::support::utils::GetMappedFileBuffer;
using ::tflite::support::utils::JStringToString;
using ::tflite::task::text::qa::BertQuestionAnswerer;
using ::tflite::task::text::qa::QaAnswer;
using ::tflite::task::text::qa::QuestionAnswerer;

constexpr int kInvalidPointer = 0;

extern "C" JNIEXPORT void JNICALL
Java_org_tensorflow_lite_task_text_qa_BertQuestionAnswerer_deinitJni(
    JNIEnv* env,
    jobject thiz,
    jlong native_handle) {
  delete reinterpret_cast<QuestionAnswerer*>(native_handle);
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_text_qa_BertQuestionAnswerer_initJniWithModelWithMetadataByteBuffers(
    JNIEnv* env,
    jclass thiz,
    jobjectArray model_buffers) {
  absl::string_view model_with_metadata =
      GetMappedFileBuffer(env, env->GetObjectArrayElement(model_buffers, 0));

  tflite::support::StatusOr<std::unique_ptr<QuestionAnswerer>> status =
      BertQuestionAnswerer::CreateFromBuffer(model_with_metadata.data(),
                                             model_with_metadata.size());
  if (status.ok()) {
    return reinterpret_cast<jlong>(status->release());
  } else {
    return kInvalidPointer;
  }
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_text_qa_BertQuestionAnswerer_initJniWithFileDescriptor(
    JNIEnv* env,
    jclass thiz,
    jint fd) {
  tflite::support::StatusOr<std::unique_ptr<QuestionAnswerer>> status =
      BertQuestionAnswerer::CreateFromFd(fd);
  if (status.ok()) {
    return reinterpret_cast<jlong>(status->release());
  } else {
    return kInvalidPointer;
  }
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_text_qa_BertQuestionAnswerer_initJniWithBertByteBuffers(
    JNIEnv* env,
    jclass thiz,
    jobjectArray model_buffers) {
  absl::string_view model =
      GetMappedFileBuffer(env, env->GetObjectArrayElement(model_buffers, 0));
  absl::string_view vocab =
      GetMappedFileBuffer(env, env->GetObjectArrayElement(model_buffers, 1));

  tflite::support::StatusOr<std::unique_ptr<QuestionAnswerer>> status =
      BertQuestionAnswerer::CreateBertQuestionAnswererFromBuffer(
          model.data(), model.size(), vocab.data(), vocab.size());
  if (status.ok()) {
    return reinterpret_cast<jlong>(status->release());
  } else {
    return kInvalidPointer;
  }
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_text_qa_BertQuestionAnswerer_initJniWithAlbertByteBuffers(
    JNIEnv* env,
    jclass thiz,
    jobjectArray model_buffers) {
  absl::string_view model =
      GetMappedFileBuffer(env, env->GetObjectArrayElement(model_buffers, 0));
  absl::string_view sp_model =
      GetMappedFileBuffer(env, env->GetObjectArrayElement(model_buffers, 1));

  tflite::support::StatusOr<std::unique_ptr<QuestionAnswerer>> status =
      BertQuestionAnswerer::CreateAlbertQuestionAnswererFromBuffer(
          model.data(), model.size(), sp_model.data(), sp_model.size());
  if (status.ok()) {
    return reinterpret_cast<jlong>(status->release());
  } else {
    return kInvalidPointer;
  }
}

extern "C" JNIEXPORT jobject JNICALL
Java_org_tensorflow_lite_task_text_qa_BertQuestionAnswerer_answerNative(
    JNIEnv* env,
    jclass thiz,
    jlong native_handle,
    jstring context,
    jstring question) {
  auto* question_answerer = reinterpret_cast<QuestionAnswerer*>(native_handle);

  std::vector<QaAnswer> results = question_answerer->Answer(
      JStringToString(env, context), JStringToString(env, question));
  jclass qa_answer_class =
      env->FindClass("org/tensorflow/lite/task/text/qa/QaAnswer");
  jmethodID qa_answer_ctor =
      env->GetMethodID(qa_answer_class, "<init>", "(Ljava/lang/String;IIF)V");

  return ConvertVectorToArrayList<QaAnswer>(
      env, results,
      [env, qa_answer_class, qa_answer_ctor](const QaAnswer& ans) {
        jstring text = env->NewStringUTF(ans.text.data());
        jobject qa_answer =
            env->NewObject(qa_answer_class, qa_answer_ctor, text, ans.pos.start,
                           ans.pos.end, ans.pos.logit);
        env->DeleteLocalRef(text);
        return qa_answer;
      });
}

}  // namespace
