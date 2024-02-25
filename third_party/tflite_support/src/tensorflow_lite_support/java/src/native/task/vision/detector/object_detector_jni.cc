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

#include "absl/strings/string_view.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/proto/base_options_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/core/frame_buffer.h"
#include "tensorflow_lite_support/cc/task/vision/object_detector.h"
#include "tensorflow_lite_support/cc/task/vision/proto/bounding_box_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/proto/detections_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/proto/object_detector_options_proto_inc.h"
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
using ::tflite::task::vision::ConvertToCategory;
using ::tflite::task::vision::DetectionResult;
using ::tflite::task::vision::FrameBuffer;
using ::tflite::task::vision::ObjectDetector;
using ::tflite::task::vision::ObjectDetectorOptions;

// Creates an ObjectDetectorOptions proto based on the Java class.
ObjectDetectorOptions ConvertToProtoOptions(JNIEnv* env, jobject java_options,
                                            jlong base_options_handle) {
  ObjectDetectorOptions proto_options;

  if (base_options_handle != kInvalidPointer) {
    // proto_options will free the previous base_options and set the new one.
    proto_options.set_allocated_base_options(
        reinterpret_cast<BaseOptions*>(base_options_handle));
  }

  jclass java_options_class = env->FindClass(
      "org/tensorflow/lite/task/vision/detector/"
      "ObjectDetector$ObjectDetectorOptions");

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
  std::vector<std::string> allow_list_vector =
      StringListToVector(env, allow_list);
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

jobject ConvertToDetectionResults(JNIEnv* env, const DetectionResult& results) {
  // jclass and init of Detection.
  jclass detection_class =
      env->FindClass("org/tensorflow/lite/task/vision/detector/Detection");
  jmethodID detection_create = env->GetStaticMethodID(
      detection_class, "create",
      "(Landroid/graphics/RectF;Ljava/util/List;)Lorg/tensorflow/lite/"
      "task/vision/detector/Detection;");

  // jclass, init, and add of ArrayList.
  jclass array_list_class = env->FindClass("java/util/ArrayList");
  jmethodID array_list_init =
      env->GetMethodID(array_list_class, "<init>", "(I)V");
  jmethodID array_list_add_method =
      env->GetMethodID(array_list_class, "add", "(Ljava/lang/Object;)Z");

  // jclass, init of RectF.
  jclass rectf_class = env->FindClass("android/graphics/RectF");
  jmethodID rectf_init = env->GetMethodID(rectf_class, "<init>", "(FFFF)V");

  jobject detections_list =
      env->NewObject(array_list_class, array_list_init,
                     static_cast<jint>(results.detections_size()));

  for (const auto& detection : results.detections()) {
    // Create the category list.
    jobject category_list = env->NewObject(array_list_class, array_list_init,
                                           detection.classes_size());
    for (const auto& classification : detection.classes()) {
      jobject jcategory = ConvertToCategory(env, classification);
      env->CallBooleanMethod(category_list, array_list_add_method, jcategory);
    }

    // Create the bounding box object.
    const BoundingBox& bounding_box = detection.bounding_box();
    float left = static_cast<float>(bounding_box.origin_x());
    float top = static_cast<float>(bounding_box.origin_y());
    float right = static_cast<float>(left + bounding_box.width());
    float bottom = static_cast<float>(top + bounding_box.height());
    jobject jbounding_box =
        env->NewObject(rectf_class, rectf_init, left, top, right, bottom);

    // Create the java Detection object.
    jobject jdetection = env->CallStaticObjectMethod(
        detection_class, detection_create, jbounding_box, category_list);
    env->CallBooleanMethod(detections_list, array_list_add_method, jdetection);
  }
  return detections_list;
}

jlong CreateObjectDetectorFromOptions(JNIEnv* env,
                                      const ObjectDetectorOptions& options) {
  StatusOr<std::unique_ptr<ObjectDetector>> object_detector_or =
      ObjectDetector::CreateFromOptions(options,
                                        tflite::task::CreateOpResolver());
  if (object_detector_or.ok()) {
    return reinterpret_cast<jlong>(object_detector_or->release());
  } else {
    ThrowException(
        env,
        GetExceptionClassNameForStatusCode(object_detector_or.status().code()),
        "Error occurred when initializing ObjectDetector: %s",
        object_detector_or.status().message().data());
    return kInvalidPointer;
  }
}

}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_org_tensorflow_lite_task_vision_detector_ObjectDetector_deinitJni(
    JNIEnv* env, jobject thiz, jlong native_handle) {
  delete reinterpret_cast<ObjectDetector*>(native_handle);
}

// Creates an ObjectDetector instance from the model file descriptor.
// file_descriptor_length and file_descriptor_offset are optional. Non-possitive
// values will be ignored.
extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_vision_detector_ObjectDetector_initJniWithModelFdAndOptions(
    JNIEnv* env, jclass thiz, jint file_descriptor,
    jlong file_descriptor_length, jlong file_descriptor_offset,
    jobject java_options, jlong base_options_handle) {
  ObjectDetectorOptions proto_options =
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
  return CreateObjectDetectorFromOptions(env, proto_options);
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_vision_detector_ObjectDetector_initJniWithByteBuffer(
    JNIEnv* env, jclass thiz, jobject model_buffer, jobject java_options,
    jlong base_options_handle) {
  ObjectDetectorOptions proto_options =
      ConvertToProtoOptions(env, java_options, base_options_handle);
  proto_options.mutable_base_options()->mutable_model_file()->set_file_content(
      static_cast<char*>(env->GetDirectBufferAddress(model_buffer)),
      static_cast<size_t>(env->GetDirectBufferCapacity(model_buffer)));
  return CreateObjectDetectorFromOptions(env, proto_options);
}

extern "C" JNIEXPORT jobject JNICALL
Java_org_tensorflow_lite_task_vision_detector_ObjectDetector_detectNative(
    JNIEnv* env, jclass thiz, jlong native_handle, jlong frame_buffer_handle) {
  auto* detector = reinterpret_cast<ObjectDetector*>(native_handle);
  // frame_buffer will be deleted after inference is done in
  // base_vision_api_jni.cc.
  auto* frame_buffer = reinterpret_cast<FrameBuffer*>(frame_buffer_handle);
  auto results_or = detector->Detect(*frame_buffer);
  if (results_or.ok()) {
    return ConvertToDetectionResults(env, results_or.value());
  } else {
    ThrowException(
        env, GetExceptionClassNameForStatusCode(results_or.status().code()),
        "Error occurred when detecting the image: %s",
        results_or.status().message().data());
    return nullptr;
  }
}
