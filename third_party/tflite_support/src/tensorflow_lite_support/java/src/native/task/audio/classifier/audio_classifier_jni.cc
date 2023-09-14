/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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

#include <memory>
#include <string>

#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/audio/audio_classifier.h"
#include "tensorflow_lite_support/cc/task/audio/core/audio_buffer.h"
#include "tensorflow_lite_support/cc/task/audio/proto/audio_classifier_options.pb.h"
#include "tensorflow_lite_support/cc/task/audio/proto/class_proto_inc.h"
#include "tensorflow_lite_support/cc/task/audio/proto/classifications_proto_inc.h"
#include "tensorflow_lite_support/cc/task/core/proto/base_options_proto_inc.h"
#include "tensorflow_lite_support/cc/utils/jni_utils.h"

namespace tflite {
namespace task {
// To be provided by a link-time library
extern std::unique_ptr<OpResolver> CreateOpResolver();

}  // namespace task
}  // namespace tflite

namespace {

using ::tflite::support::StatusOr;
using ::tflite::support::utils::GetExceptionClassNameForStatusCode;
using ::tflite::support::utils::kIllegalArgumentException;
using ::tflite::support::utils::kInvalidPointer;
using ::tflite::support::utils::StringListToVector;
using ::tflite::support::utils::ThrowException;
using ::tflite::task::audio::AudioBuffer;
using ::tflite::task::audio::AudioClassifier;
using ::tflite::task::audio::AudioClassifierOptions;
using ::tflite::task::audio::Class;
using ::tflite::task::audio::ClassificationResult;
using ::tflite::task::core::BaseOptions;

// TODO(b/183343074): Share the code below with ImageClassifier.

constexpr char kCategoryClassName[] =
    "org/tensorflow/lite/support/label/Category";
constexpr char kStringClassName[] = "Ljava/lang/String;";
constexpr char kEmptyString[] = "";

jobject ConvertToCategory(JNIEnv* env, const Class& classification) {
  // jclass and init of Category.
  jclass category_class = env->FindClass(kCategoryClassName);
  jmethodID category_create = env->GetStaticMethodID(
      category_class, "create",
      absl::StrCat("(", kStringClassName, kStringClassName, "FI)L",
                   kCategoryClassName, ";")
          .c_str());

  std::string label_string = classification.has_class_name()
                                 ? classification.class_name()
                                 : std::to_string(classification.index());
  jstring label = env->NewStringUTF(label_string.c_str());
  std::string display_name_string = classification.has_display_name()
                                        ? classification.display_name()
                                        : kEmptyString;
  jstring display_name = env->NewStringUTF(display_name_string.c_str());
  jobject jcategory = env->CallStaticObjectMethod(
      category_class, category_create, label, display_name,
      classification.score(), classification.index());
  env->DeleteLocalRef(category_class);
  env->DeleteLocalRef(label);
  env->DeleteLocalRef(display_name);
  return jcategory;
}

jobject ConvertToClassificationResults(JNIEnv* env,
                                       const ClassificationResult& results) {
  // jclass and init of Classifications.
  jclass classifications_class = env->FindClass(
      "org/tensorflow/lite/task/audio/classifier/Classifications");
  jmethodID classifications_create = env->GetStaticMethodID(
      classifications_class, "create",
      "(Ljava/util/List;ILjava/lang/String;)Lorg/tensorflow/lite/"
      "task/audio/classifier/Classifications;");

  // jclass, init, and add of ArrayList.
  jclass array_list_class = env->FindClass("java/util/ArrayList");
  jmethodID array_list_init =
      env->GetMethodID(array_list_class, "<init>", "(I)V");
  jmethodID array_list_add_method =
      env->GetMethodID(array_list_class, "add", "(Ljava/lang/Object;)Z");

  jobject classifications_list =
      env->NewObject(array_list_class, array_list_init,
                     static_cast<jint>(results.classifications_size()));
  for (int i = 0; i < results.classifications_size(); i++) {
    auto classifications = results.classifications(i);
    jobject jcategory_list = env->NewObject(array_list_class, array_list_init,
                                            classifications.classes_size());
    for (const auto& classification : classifications.classes()) {
      jobject jcategory = ConvertToCategory(env, classification);
      env->CallBooleanMethod(jcategory_list, array_list_add_method, jcategory);

      env->DeleteLocalRef(jcategory);
    }

    std::string head_name_string =
        classifications.has_head_name()
            ? classifications.head_name()
            : std::to_string(classifications.head_index());
    jstring head_name = env->NewStringUTF(head_name_string.c_str());

    jobject jclassifications = env->CallStaticObjectMethod(
        classifications_class, classifications_create, jcategory_list,
        classifications.head_index(), head_name);
    env->CallBooleanMethod(classifications_list, array_list_add_method,
                           jclassifications);

    env->DeleteLocalRef(head_name);
    env->DeleteLocalRef(jcategory_list);
    env->DeleteLocalRef(jclassifications);
  }
  return classifications_list;
}

// Creates an AudioClassifierOptions proto based on the Java class.
AudioClassifierOptions ConvertToProtoOptions(JNIEnv* env, jobject java_options,
                                             jlong base_options_handle) {
  AudioClassifierOptions proto_options;

  if (base_options_handle != kInvalidPointer) {
    // proto_options will free the previous base_options and set the new one.
    proto_options.set_allocated_base_options(
        reinterpret_cast<BaseOptions*>(base_options_handle));
  }

  jclass java_options_class = env->FindClass(
      "org/tensorflow/lite/task/audio/classifier/"
      "AudioClassifier$AudioClassifierOptions");

  jmethodID display_names_locale_id = env->GetMethodID(
      java_options_class, "getDisplayNamesLocale", "()Ljava/lang/String;");
  jstring display_names_locale = static_cast<jstring>(
      env->CallObjectMethod(java_options, display_names_locale_id));
  const char* pchars = env->GetStringUTFChars(display_names_locale, nullptr);
  proto_options.set_display_names_locale(pchars);
  env->ReleaseStringUTFChars(display_names_locale, pchars);

  jmethodID max_results_id =
      env->GetMethodID(java_options_class, "getMaxResults", "()I");
  jint max_results = env->CallIntMethod(java_options, max_results_id);
  proto_options.set_max_results(max_results);

  jmethodID is_score_threshold_set_id =
      env->GetMethodID(java_options_class, "getIsScoreThresholdSet", "()Z");
  jboolean is_score_threshold_set =
      env->CallBooleanMethod(java_options, is_score_threshold_set_id);
  if (is_score_threshold_set) {
    jmethodID score_threshold_id =
        env->GetMethodID(java_options_class, "getScoreThreshold", "()F");
    jfloat score_threshold =
        env->CallFloatMethod(java_options, score_threshold_id);
    proto_options.set_score_threshold(score_threshold);
  }

  jmethodID allow_list_id = env->GetMethodID(
      java_options_class, "getLabelAllowList", "()Ljava/util/List;");
  jobject allow_list = env->CallObjectMethod(java_options, allow_list_id);
  auto allow_list_vector = StringListToVector(env, allow_list);
  for (const auto& class_name : allow_list_vector) {
    proto_options.add_class_name_allowlist(class_name);
  }

  jmethodID deny_list_id = env->GetMethodID(
      java_options_class, "getLabelDenyList", "()Ljava/util/List;");
  jobject deny_list = env->CallObjectMethod(java_options, deny_list_id);
  auto deny_list_vector = StringListToVector(env, deny_list);
  for (const auto& class_name : deny_list_vector) {
    proto_options.add_class_name_denylist(class_name);
  }

  return proto_options;
}

jlong CreateAudioClassifierFromOptions(JNIEnv* env,
                                       const AudioClassifierOptions& options) {
  StatusOr<std::unique_ptr<AudioClassifier>> audio_classifier_or =
      AudioClassifier::CreateFromOptions(options,
                                         tflite::task::CreateOpResolver());
  if (audio_classifier_or.ok()) {
    // Deletion is handled at deinitJni time.
    return reinterpret_cast<jlong>(audio_classifier_or->release());
  } else {
    ThrowException(
        env,
        GetExceptionClassNameForStatusCode(audio_classifier_or.status().code()),
        "Error occurred when initializing AudioClassifier: %s",
        audio_classifier_or.status().message().data());
  }
  return kInvalidPointer;
}

}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_org_tensorflow_lite_task_audio_classifier_AudioClassifier_deinitJni(
    JNIEnv* env, jobject thiz, jlong native_handle) {
  delete reinterpret_cast<AudioClassifier*>(native_handle);
}

// Creates an AudioClassifier instance from the model file descriptor.
// file_descriptor_length and file_descriptor_offset are optional. Non-possitive
// values will be ignored.
extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_audio_classifier_AudioClassifier_initJniWithModelFdAndOptions(
    JNIEnv* env, jclass thiz, jint file_descriptor,
    jlong file_descriptor_length, jlong file_descriptor_offset,
    jobject java_options, jlong base_options_handle) {
  AudioClassifierOptions proto_options =
      ConvertToProtoOptions(env, java_options, base_options_handle);
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
  return CreateAudioClassifierFromOptions(env, proto_options);
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_audio_classifier_AudioClassifier_initJniWithByteBuffer(
    JNIEnv* env, jclass thiz, jobject model_buffer, jobject java_options,
    jlong base_options_handle) {
  AudioClassifierOptions proto_options =
      ConvertToProtoOptions(env, java_options, base_options_handle);
  // External proto generated header does not overload `set_file_content` with
  // string_view, therefore GetMappedFileBuffer does not apply here.
  // Creating a std::string will cause one extra copying of data. Thus, the
  // most efficient way here is to set file_content using char* and its size.
  proto_options.mutable_base_options()->mutable_model_file()->set_file_content(
      static_cast<char*>(env->GetDirectBufferAddress(model_buffer)),
      static_cast<size_t>(env->GetDirectBufferCapacity(model_buffer)));
  return CreateAudioClassifierFromOptions(env, proto_options);
}

// TODO(b/183343074): JNI method invocation is very expensive, taking about .2ms
// each time. Consider retrieving the AudioFormat during initialization and
// caching it in JAVA layer.
extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_audio_classifier_AudioClassifier_getRequiredSampleRateNative(
    JNIEnv* env, jclass thiz, jlong native_handle) {
  auto* classifier = reinterpret_cast<AudioClassifier*>(native_handle);
  StatusOr<AudioBuffer::AudioFormat> format_or =
      classifier->GetRequiredAudioFormat();
  if (format_or.ok()) {
    return format_or->sample_rate;
  } else {
    ThrowException(
        env, GetExceptionClassNameForStatusCode(format_or.status().code()),
        "Error occurred when getting sample rate from AudioClassifier: %s",
        format_or.status().message());
    return kInvalidPointer;
  }
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_audio_classifier_AudioClassifier_getRequiredChannelsNative(
    JNIEnv* env, jclass thiz, jlong native_handle) {
  auto* classifier = reinterpret_cast<AudioClassifier*>(native_handle);
  StatusOr<AudioBuffer::AudioFormat> format_or =
      classifier->GetRequiredAudioFormat();
  if (format_or.ok()) {
    return format_or->channels;
  } else {
    ThrowException(
        env, GetExceptionClassNameForStatusCode(format_or.status().code()),
        "Error occurred when gettng channels from AudioClassifier: %s",
        format_or.status().message());
    return kInvalidPointer;
  }
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_audio_classifier_AudioClassifier_getRequiredInputBufferSizeNative(
    JNIEnv* env, jclass thiz, jlong native_handle) {
  auto* classifier = reinterpret_cast<AudioClassifier*>(native_handle);
  return classifier->GetRequiredInputBufferSize();
}

extern "C" JNIEXPORT jobject JNICALL
Java_org_tensorflow_lite_task_audio_classifier_AudioClassifier_classifyNative(
    JNIEnv* env, jclass thiz, jlong native_handle, jbyteArray java_array,
    jint channels, jint sample_rate) {
  // Get the primitive native array. Depending on the JAVA runtime, the returned
  // array might be a copy of the JAVA array (or not).
  jbyte* native_array = env->GetByteArrayElements(java_array, nullptr);
  if (native_array == nullptr) {
    ThrowException(env, kIllegalArgumentException,
                   "Error occurred when converting the java audio input array "
                   "to native array.");
    return nullptr;
  }

  jobject classification_results = nullptr;

  // Prepare the AudioBuffer.
  AudioBuffer::AudioFormat format = {channels, sample_rate};
  const int size_in_bytes = env->GetArrayLength(java_array);
  const int size_in_float = size_in_bytes / sizeof(float);
  const StatusOr<std::unique_ptr<AudioBuffer>> audio_buffer_or =
      AudioBuffer::Create(reinterpret_cast<float*>(native_array), size_in_float,
                          format);

  if (audio_buffer_or.ok()) {
    // Actual classification
    auto* classifier = reinterpret_cast<AudioClassifier*>(native_handle);
    auto results_or = classifier->Classify(*(audio_buffer_or.value()));
    if (results_or.ok()) {
      classification_results =
          ConvertToClassificationResults(env, results_or.value());
    } else {
      ThrowException(
          env, GetExceptionClassNameForStatusCode(results_or.status().code()),
          "Error occurred when classifying the audio clip: %s",
          results_or.status().message().data());
    }
  } else {
    ThrowException(
        env,
        GetExceptionClassNameForStatusCode(audio_buffer_or.status().code()),
        "Error occurred when creating the AudioBuffer: %s",
        audio_buffer_or.status().message().data());
  }

  // Mark native_array as no longer needed.
  // TODO(b/183343074): Wrap this in SimpleCleanUp.
  env->ReleaseByteArrayElements(java_array, native_array, /*mode=*/0);
  return classification_results;
}
