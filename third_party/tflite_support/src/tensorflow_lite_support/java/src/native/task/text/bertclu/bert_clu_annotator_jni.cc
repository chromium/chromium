/* Copyright 2022 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/proto/class.pb.h"
#include "tensorflow_lite_support/cc/task/text/bert_clu_annotator.h"
#include "tensorflow_lite_support/cc/task/text/clu_annotator.h"
#include "tensorflow_lite_support/cc/task/text/proto/bert_clu_annotator_options.pb.h"
#include "tensorflow_lite_support/cc/task/text/proto/clu.pb.h"
#include "tensorflow_lite_support/cc/utils/jni_utils.h"

namespace {

using ::tflite::support::StatusOr;
using ::tflite::support::utils::ConvertVectorToArrayList;
using ::tflite::support::utils::GetExceptionClassNameForStatusCode;
using ::tflite::support::utils::kInvalidPointer;
using ::tflite::support::utils::StringListToVector;
using ::tflite::support::utils::ThrowException;
using ::tflite::task::core::BaseOptions;
using ::tflite::task::core::Class;
using ::tflite::task::text::BertCluAnnotatorOptions;
using ::tflite::task::text::CategoricalSlot;
using ::tflite::task::text::CluRequest;
using ::tflite::task::text::CluResponse;
using ::tflite::task::text::Mention;
using ::tflite::task::text::MentionedSlot;
using ::tflite::task::text::clu::BertCluAnnotator;
using ::tflite::task::text::clu::CluAnnotator;

// Creates a BertCluAnnotatorOptions proto based on the Java class.
BertCluAnnotatorOptions ConvertJavaBertCluAnnotatorProtoOptionsToCpp(
    JNIEnv* env, jobject java_bert_clu_annotator_options,
    jlong base_options_handle) {
  static jclass bert_clu_annotator_options_class =
      static_cast<jclass>(env->NewGlobalRef(
          env->FindClass("org/tensorflow/lite/task/text/bertclu/"
                         "BertCluAnnotator$BertCluAnnotatorOptions")));
  static jmethodID max_history_turns_method_id = env->GetMethodID(
      bert_clu_annotator_options_class, "getMaxHistoryTurns", "()I");
  static jmethodID domain_threshold_method_id = env->GetMethodID(
      bert_clu_annotator_options_class, "getDomainThreshold", "()F");
  static jmethodID intent_threshold_method_id = env->GetMethodID(
      bert_clu_annotator_options_class, "getIntentThreshold", "()F");
  static jmethodID categorical_slot_threshold_method_id = env->GetMethodID(
      bert_clu_annotator_options_class, "getCategoricalSlotThreshold", "()F");
  static jmethodID mentioned_slot_threshold_method_id =
      env->GetMethodID(bert_clu_annotator_options_class,
                       "getMentionedSlotThreshold", "()F");
  BertCluAnnotatorOptions proto_options;

  if (base_options_handle != kInvalidPointer) {
    // proto_options will free the previous base_options and set the new one.
    proto_options.set_allocated_base_options(
        reinterpret_cast<BaseOptions*>(base_options_handle));
  }
  proto_options.set_max_history_turns(env->CallIntMethod(
      java_bert_clu_annotator_options, max_history_turns_method_id));
  proto_options.set_domain_threshold(env->CallFloatMethod(
      java_bert_clu_annotator_options, domain_threshold_method_id));
  proto_options.set_intent_threshold(env->CallFloatMethod(
      java_bert_clu_annotator_options, intent_threshold_method_id));
  proto_options.set_categorical_slot_threshold(env->CallFloatMethod(
      java_bert_clu_annotator_options, categorical_slot_threshold_method_id));
  proto_options.set_mentioned_slot_threshold(
      env->CallFloatMethod(java_bert_clu_annotator_options,
                           mentioned_slot_threshold_method_id));

  return proto_options;
}

// Creates a CluRequest proto based on the Java class.
CluRequest ConvertJavaCluRequestToCpp(JNIEnv* env, jobject java_clu_request) {
  static jclass clu_request_class = static_cast<jclass>(env->NewGlobalRef(
      env->FindClass("org/tensorflow/lite/task/text/bertclu/CluRequest")));
  static jmethodID utterances_method_id = env->GetMethodID(
      clu_request_class, "getUtterances", "()Ljava/util/List;");
  jobject java_utterances =
      env->CallObjectMethod(java_clu_request, utterances_method_id);
  std::vector<std::string> utterances =
      StringListToVector(env, java_utterances);
  CluRequest clu_request;
  clu_request.mutable_utterances()->Reserve(utterances.size());
  for (const std::string& utterance : utterances) {
    clu_request.add_utterances(std::move(utterance));
  }
  env->DeleteLocalRef(java_utterances);
  return clu_request;
}

// Builds a Java Category based on the Cpp Class proto.
jobject ConvertCppCategoryToJava(JNIEnv* env, const Class& category) {
  static jclass category_class = static_cast<jclass>(env->NewGlobalRef(
      env->FindClass("org/tensorflow/lite/support/label/Category")));
  static jmethodID category_create_method_id =
      env->GetStaticMethodID(category_class, "create",
                             "(Ljava/lang/String;Ljava/lang/String;FI)Lorg/"
                             "tensorflow/lite/support/label/Category;");

  jstring java_display_name =
      env->NewStringUTF(category.display_name().c_str());
  jstring java_class_name = env->NewStringUTF(category.class_name().c_str());
  jobject java_category = env->CallStaticObjectMethod(
      category_class, category_create_method_id, java_class_name,
      java_display_name, category.score(), category.index());

  env->DeleteLocalRef(java_display_name);
  env->DeleteLocalRef(java_class_name);
  return java_category;
}

// Builds a Java List of CategoricalSlots based on the input `clu_response`.
jobject ConvertCppCategoricalSlotsToJava(JNIEnv* env,
                                         const CluResponse& clu_response) {
  static jclass categorical_slot_class = static_cast<
      jclass>(env->NewGlobalRef(env->FindClass(
      "org/tensorflow/lite/task/text/bertclu/CluResponse$CategoricalSlot")));
  static jmethodID categorical_slot_create_method_id = env->GetStaticMethodID(
      categorical_slot_class, "create",
      "(Ljava/lang/String;Lorg/tensorflow/lite/support/label/Category;)Lorg/"
      "tensorflow/lite/task/text/bertclu/CluResponse$CategoricalSlot;");
  return ConvertVectorToArrayList(
      env, clu_response.categorical_slots().begin(),
      clu_response.categorical_slots().end(),
      [env](const CategoricalSlot& categorical_slot) {
        jstring java_slot = env->NewStringUTF(categorical_slot.slot().c_str());
        jobject java_prediction =
            ConvertCppCategoryToJava(env, categorical_slot.prediction());
        jobject java_categorical_slots = env->CallStaticObjectMethod(
            categorical_slot_class, categorical_slot_create_method_id,
            java_slot, java_prediction);

        env->DeleteLocalRef(java_slot);
        env->DeleteLocalRef(java_prediction);
        return java_categorical_slots;
      });
}

// Builds a Java List of `MentionedSlot`s based on the input `clu_response`.
jobject ConvertCppMentionedSlotsToJava(JNIEnv* env,
                                            const CluResponse& clu_response) {
  static jclass mention_class =
      static_cast<jclass>(env->NewGlobalRef(env->FindClass(
          "org/tensorflow/lite/task/text/bertclu/CluResponse$Mention")));
  static jmethodID mention_create_method_id =
      env->GetStaticMethodID(mention_class, "create",
                             "(Ljava/lang/String;FII)Lorg/tensorflow/lite/task/"
                             "text/bertclu/CluResponse$Mention;");
  static jclass mentioned_slot_class = static_cast<
      jclass>(env->NewGlobalRef(env->FindClass(
      "org/tensorflow/lite/task/text/bertclu/CluResponse$MentionedSlot")));
  static jmethodID mentioned_slot_create_method_id =
      env->GetStaticMethodID(
          mentioned_slot_class, "create",
          "(Ljava/lang/String;Lorg/tensorflow/lite/task/text/bertclu/"
          "CluResponse$Mention;)Lorg/tensorflow/lite/task/text/bertclu/"
          "CluResponse$MentionedSlot;");

  return ConvertVectorToArrayList(
      env, clu_response.mentioned_slots().begin(),
      clu_response.mentioned_slots().end(),
      [env](const MentionedSlot& mentioned_slot) {
        Mention mention = mentioned_slot.mention();
        jobject java_mention = env->CallStaticObjectMethod(
            mention_class, mention_create_method_id,
            env->NewStringUTF(mention.value().c_str()), mention.score(),
            mention.start(), mention.end());
        jstring java_slot =
            env->NewStringUTF(mentioned_slot.slot().c_str());
        jobject java_mentioned_slots = env->CallStaticObjectMethod(
            mentioned_slot_class, mentioned_slot_create_method_id,
            java_slot, java_mention);

        env->DeleteLocalRef(java_mention);
        env->DeleteLocalRef(java_slot);
        return java_mentioned_slots;
      });
}

// Builds a Java CluResponse based on the input CluResponse proto.
jobject ConvertCppCluResponseToJava(JNIEnv* env,
                                    const CluResponse& clu_response) {
  static jclass clu_response_class = static_cast<jclass>(env->NewGlobalRef(
      env->FindClass("org/tensorflow/lite/task/text/bertclu/CluResponse")));
  static jmethodID clu_response_create_method_id = env->GetStaticMethodID(
      clu_response_class, "create",
      "(Ljava/util/List;Ljava/util/List;Ljava/util/List;Ljava/util/List;)Lorg/"
      "tensorflow/lite/task/text/bertclu/CluResponse;");

  jobject java_domains = ConvertVectorToArrayList(
      env, clu_response.domains().begin(), clu_response.domains().end(),
      [env](const Class& category) {
        return ConvertCppCategoryToJava(env, category);
      });
  jobject java_intents = ConvertVectorToArrayList(
      env, clu_response.intents().begin(), clu_response.intents().end(),
      [env](const Class& category) {
        return ConvertCppCategoryToJava(env, category);
      });
  jobject java_categorical_slots =
      ConvertCppCategoricalSlotsToJava(env, clu_response);
  jobject java_mentioned_slots =
      ConvertCppMentionedSlotsToJava(env, clu_response);
  jobject java_clu_response = env->CallStaticObjectMethod(
      clu_response_class, clu_response_create_method_id, java_domains,
      java_intents, java_categorical_slots, java_mentioned_slots);

  env->DeleteLocalRef(java_domains);
  env->DeleteLocalRef(java_intents);
  env->DeleteLocalRef(java_categorical_slots);
  env->DeleteLocalRef(java_mentioned_slots);
  return java_clu_response;
}

}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_org_tensorflow_lite_task_text_bertclu_BertCluAnnotator_deinitJni(
    JNIEnv* env, jobject thiz, jlong native_handle) {
  delete reinterpret_cast<CluAnnotator*>(native_handle);
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_text_bertclu_BertCluAnnotator_initJniWithByteBuffer(
    JNIEnv* env, jclass thiz, jobject bert_clu_annotator_options,
    jobject model_buffer, jlong base_options_handle) {
  BertCluAnnotatorOptions proto_options =
      ConvertJavaBertCluAnnotatorProtoOptionsToCpp(
          env, bert_clu_annotator_options, base_options_handle);
  proto_options.mutable_base_options()->mutable_model_file()->set_file_content(
      static_cast<char*>(env->GetDirectBufferAddress(model_buffer)),
      static_cast<size_t>(env->GetDirectBufferCapacity(model_buffer)));

  tflite::support::StatusOr<std::unique_ptr<CluAnnotator>> clu_annotator =
      BertCluAnnotator::CreateFromOptions(proto_options);
  if (clu_annotator.ok()) {
    return reinterpret_cast<jlong>(clu_annotator->release());
  } else {
    ThrowException(
        env, GetExceptionClassNameForStatusCode(clu_annotator.status().code()),
        "Error occurred when initializing BertCluAnnotator: %s",
        clu_annotator.status().message().data());
    return kInvalidPointer;
  }
}

extern "C" JNIEXPORT jobject JNICALL
Java_org_tensorflow_lite_task_text_bertclu_BertCluAnnotator_annotateNative(
    JNIEnv* env, jclass thiz, jlong native_handle, jobject java_clu_request) {
  CluRequest clu_request = ConvertJavaCluRequestToCpp(env, java_clu_request);
  auto* bert_clu_annotator = reinterpret_cast<BertCluAnnotator*>(native_handle);
  absl::StatusOr<CluResponse> clu_response =
      bert_clu_annotator->Annotate(clu_request);
  if (!clu_response.ok()) {
    ThrowException(
        env, GetExceptionClassNameForStatusCode(clu_response.status().code()),
        "Error occurred during BERT CLU annotation: %s",
        clu_response.status().message().data());
  }
  return ConvertCppCluResponseToJava(env, *clu_response);
}
