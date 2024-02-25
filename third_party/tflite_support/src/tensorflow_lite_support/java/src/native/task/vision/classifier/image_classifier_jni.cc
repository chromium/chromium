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

#include <memory>
#include <string>

#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/proto/base_options_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/core/frame_buffer.h"
#include "tensorflow_lite_support/cc/task/vision/image_classifier.h"
#include "tensorflow_lite_support/cc/task/vision/proto/bounding_box_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/proto/classifications_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/proto/image_classifier_options_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_common_utils.h"
#include "tensorflow_lite_support/cc/utils/jni_utils.h"
#include "tensorflow_lite_support/java/src/native/task/vision/jni_utils.h"

namespace tflite {
namespace task {
// To be provided by a link-time library
extern std::unique_ptr<OpResolver> CreateOpResolver();

}  // namespace task
}  // namespace tflite

namespace {

using ::tflite::support::StatusOr;
using ::tflite::support::utils::GetExceptionClassNameForStatusCode;
using ::tflite::support::utils::kInvalidPointer;
using ::tflite::support::utils::StringListToVector;
using ::tflite::support::utils::ThrowException;
using ::tflite::task::core::BaseOptions;
using ::tflite::task::vision::BoundingBox;
using ::tflite::task::vision::ClassificationResult;
using ::tflite::task::vision::Classifications;
using ::tflite::task::vision::ConvertToCategory;
using ::tflite::task::vision::FrameBuffer;
using ::tflite::task::vision::ImageClassifier;
using ::tflite::task::vision::ImageClassifierOptions;

// Creates an ImageClassifierOptions proto based on the Java class.
ImageClassifierOptions ConvertToProtoOptions(JNIEnv* env, jobject java_options,
                                             jlong base_options_handle) {
  ImageClassifierOptions proto_options;

  if (base_options_handle != kInvalidPointer) {
    // proto_options will free the previous base_options and set the new one.
    proto_options.set_allocated_base_options(
        reinterpret_cast<BaseOptions*>(base_options_handle));
  }

  jclass java_options_class = env->FindClass(
      "org/tensorflow/lite/task/vision/classifier/"
      "ImageClassifier$ImageClassifierOptions");

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
    proto_options.add_class_name_whitelist(class_name);
  }

  jmethodID deny_list_id = env->GetMethodID(
      java_options_class, "getLabelDenyList", "()Ljava/util/List;");
  jobject deny_list = env->CallObjectMethod(java_options, deny_list_id);
  auto deny_list_vector = StringListToVector(env, deny_list);
  for (const auto& class_name : deny_list_vector) {
    proto_options.add_class_name_blacklist(class_name);
  }
  return proto_options;
}

jobject ConvertToClassificationResults(JNIEnv* env,
                                       const ClassificationResult& results) {
  // jclass and init of Classifications.
  jclass classifications_class = env->FindClass(
      "org/tensorflow/lite/task/vision/classifier/Classifications");
  jmethodID classifications_create =
      env->GetStaticMethodID(classifications_class, "create",
                             "(Ljava/util/List;I)Lorg/tensorflow/lite/"
                             "task/vision/classifier/Classifications;");

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
    jobject jclassifications = env->CallStaticObjectMethod(
        classifications_class, classifications_create, jcategory_list,
        classifications.head_index());
    env->CallBooleanMethod(classifications_list, array_list_add_method,
                           jclassifications);

    env->DeleteLocalRef(jcategory_list);
    env->DeleteLocalRef(jclassifications);
  }
  return classifications_list;
}

jlong CreateImageClassifierFromOptions(JNIEnv* env,
                                       const ImageClassifierOptions& options) {
  StatusOr<std::unique_ptr<ImageClassifier>> image_classifier_or =
      ImageClassifier::CreateFromOptions(options,
                                         tflite::task::CreateOpResolver());
  if (image_classifier_or.ok()) {
    // Deletion is handled at deinitJni time.
    return reinterpret_cast<jlong>(image_classifier_or->release());
  } else {
    ThrowException(
        env,
        GetExceptionClassNameForStatusCode(image_classifier_or.status().code()),
        "Error occurred when initializing ImageClassifier: %s",
        image_classifier_or.status().message().data());
    return kInvalidPointer;
  }
}

}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_org_tensorflow_lite_task_vision_classifier_ImageClassifier_deinitJni(
    JNIEnv* env, jobject thiz, jlong native_handle) {
  delete reinterpret_cast<ImageClassifier*>(native_handle);
}

// Creates an ImageClassifier instance from the model file descriptor.
// file_descriptor_length and file_descriptor_offset are optional. Non-possitive
// values will be ignored.
extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_vision_classifier_ImageClassifier_initJniWithModelFdAndOptions(
    JNIEnv* env, jclass thiz, jint file_descriptor,
    jlong file_descriptor_length, jlong file_descriptor_offset,
    jobject java_options, jlong base_options_handle) {
  ImageClassifierOptions proto_options =
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
  return CreateImageClassifierFromOptions(env, proto_options);
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_vision_classifier_ImageClassifier_initJniWithByteBuffer(
    JNIEnv* env, jclass thiz, jobject model_buffer, jobject java_options,
    jlong base_options_handle) {
  ImageClassifierOptions proto_options =
      ConvertToProtoOptions(env, java_options, base_options_handle);
  // External proto generated header does not overload `set_file_content` with
  // string_view, therefore GetMappedFileBuffer does not apply here.
  // Creating a std::string will cause one extra copying of data. Thus, the
  // most efficient way here is to set file_content using char* and its size.
  proto_options.mutable_base_options()->mutable_model_file()->set_file_content(
      static_cast<char*>(env->GetDirectBufferAddress(model_buffer)),
      static_cast<size_t>(env->GetDirectBufferCapacity(model_buffer)));
  return CreateImageClassifierFromOptions(env, proto_options);
}

extern "C" JNIEXPORT jobject JNICALL
Java_org_tensorflow_lite_task_vision_classifier_ImageClassifier_classifyNative(
    JNIEnv* env, jclass thiz, jlong native_handle, jlong frame_buffer_handle,
    jintArray jroi) {
  auto* classifier = reinterpret_cast<ImageClassifier*>(native_handle);
  // frame_buffer will be deleted after inference is done in
  // base_vision_api_jni.cc.
  auto* frame_buffer = reinterpret_cast<FrameBuffer*>(frame_buffer_handle);

  int* roi_array = env->GetIntArrayElements(jroi, 0);
  BoundingBox roi;
  roi.set_origin_x(roi_array[0]);
  roi.set_origin_y(roi_array[1]);
  roi.set_width(roi_array[2]);
  roi.set_height(roi_array[3]);
  env->ReleaseIntArrayElements(jroi, roi_array, 0);

  auto results_or = classifier->Classify(*frame_buffer, roi);
  if (results_or.ok()) {
    return ConvertToClassificationResults(env, results_or.value());
  } else {
    ThrowException(
        env, GetExceptionClassNameForStatusCode(results_or.status().code()),
        "Error occurred when classifying the image: %s",
        results_or.status().message().data());
    return nullptr;
  }
}
