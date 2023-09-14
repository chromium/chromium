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

#include <memory>

#include "tensorflow_lite_support/cc/port/statusor.h"
#include "tensorflow_lite_support/cc/task/core/proto/base_options_proto_inc.h"
#include "tensorflow_lite_support/cc/task/processor/proto/search_result.pb.h"
#include "tensorflow_lite_support/cc/task/vision/core/frame_buffer.h"
#include "tensorflow_lite_support/cc/task/vision/image_searcher.h"
#include "tensorflow_lite_support/cc/task/vision/proto/image_searcher_options.pb.h"
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
using ::tflite::support::utils::ConvertVectorToArrayList;
using ::tflite::support::utils::CreateByteArray;
using ::tflite::support::utils::GetExceptionClassNameForStatusCode;
using ::tflite::support::utils::kInvalidPointer;
using ::tflite::support::utils::ThrowException;
using ::tflite::task::core::BaseOptions;
using ::tflite::task::processor::NearestNeighbor;
using ::tflite::task::processor::SearchResult;
using ::tflite::task::vision::BoundingBox;
using ::tflite::task::vision::FrameBuffer;
using ::tflite::task::vision::ImageSearcher;
using ::tflite::task::vision::ImageSearcherOptions;

// Creates an ImageSearcherOptions proto based on the Java class.
ImageSearcherOptions ConvertToProtoOptions(jlong base_options_handle,
                                           bool l2_normalize, bool quantize,
                                           int index_descriptor,
                                           int max_results) {
  ImageSearcherOptions proto_options;

  if (base_options_handle != kInvalidPointer) {
    // proto_options will free the previous base_options and set the new one.
    proto_options.set_allocated_base_options(
        reinterpret_cast<BaseOptions*>(base_options_handle));
  }

  auto embedding_options = proto_options.mutable_embedding_options();
  embedding_options->set_l2_normalize(l2_normalize);
  embedding_options->set_quantize(quantize);

  auto search_options = proto_options.mutable_search_options();
  if (index_descriptor > 0) {
    search_options->mutable_index_file()
        ->mutable_file_descriptor_meta()
        ->set_fd(index_descriptor);
  }
  search_options->set_max_results(max_results);

  return proto_options;
}

jlong CreateImageSearcherFromOptions(JNIEnv* env,
                                     const ImageSearcherOptions& options) {
  StatusOr<std::unique_ptr<ImageSearcher>> image_searcher_or =
      ImageSearcher::CreateFromOptions(options,
                                       tflite::task::CreateOpResolver());
  if (image_searcher_or.ok()) {
    return reinterpret_cast<jlong>(image_searcher_or->release());
  } else {
    ThrowException(
        env,
        GetExceptionClassNameForStatusCode(image_searcher_or.status().code()),
        "Error occurred when initializing ImageSearcher: %s",
        image_searcher_or.status().message().data());
    return kInvalidPointer;
  }
}

jobject ConvertToSearchResults(JNIEnv* env, const SearchResult& results) {
  // jclass and factory create of NearestNeighbor.
  jclass nearest_neighbor_class =
      env->FindClass("org/tensorflow/lite/task/processor/NearestNeighbor");
  jmethodID nearest_neighbor_create =
      env->GetStaticMethodID(nearest_neighbor_class, "create",
                             "([BF)Lorg/tensorflow/lite/"
                             "task/processor/NearestNeighbor;");

  return ConvertVectorToArrayList(
      env, results.nearest_neighbors().begin(),
      results.nearest_neighbors().end(),
      [env, nearest_neighbor_class,
       nearest_neighbor_create](const NearestNeighbor& neightbor) {
        jbyteArray jmetadata = CreateByteArray(
            env, reinterpret_cast<const jbyte*>(neightbor.metadata().data()),
            neightbor.metadata().size());
        jobject jnearest_neighbor = env->CallStaticObjectMethod(
            nearest_neighbor_class, nearest_neighbor_create, jmetadata,
            neightbor.distance());
        env->DeleteLocalRef(jmetadata);
        return jnearest_neighbor;
      });
}

}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_org_tensorflow_lite_task_vision_searcher_ImageSearcher_deinitJni(
    JNIEnv* env, jobject thiz, jlong native_handle) {
  delete reinterpret_cast<ImageSearcher*>(native_handle);
}

// Creates an ImageSearcher instance from the model file descriptor.
// file_descriptor_length and file_descriptor_offset are optional. Non-positive
// values will be ignored.
extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_vision_searcher_ImageSearcher_initJniWithModelFdAndOptions(
    JNIEnv* env, jclass thiz, jint model_descriptor,
    jlong model_descriptor_length, jlong model_descriptor_offset,
    jlong base_options_handle, bool l2_normalize, bool quantize,
    jint index_descriptor, int max_results) {
  ImageSearcherOptions proto_options =
      ConvertToProtoOptions(base_options_handle, l2_normalize, quantize,
                            index_descriptor, max_results);
  auto file_descriptor_meta = proto_options.mutable_base_options()
                                  ->mutable_model_file()
                                  ->mutable_file_descriptor_meta();
  file_descriptor_meta->set_fd(model_descriptor);
  if (model_descriptor_length > 0) {
    file_descriptor_meta->set_length(model_descriptor_length);
  }
  if (model_descriptor_offset > 0) {
    file_descriptor_meta->set_offset(model_descriptor_offset);
  }

  return CreateImageSearcherFromOptions(env, proto_options);
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_vision_searcher_ImageSearcher_initJniWithByteBuffer(
    JNIEnv* env, jclass thiz, jobject model_buffer, jlong base_options_handle,
    bool l2_normalize, bool quantize, jlong index_descriptor, int max_results) {
  ImageSearcherOptions proto_options =
      ConvertToProtoOptions(base_options_handle, l2_normalize, quantize,
                            index_descriptor, max_results);
  proto_options.mutable_base_options()->mutable_model_file()->set_file_content(
      static_cast<char*>(env->GetDirectBufferAddress(model_buffer)),
      static_cast<size_t>(env->GetDirectBufferCapacity(model_buffer)));

  return CreateImageSearcherFromOptions(env, proto_options);
}

extern "C" JNIEXPORT jobject JNICALL
Java_org_tensorflow_lite_task_vision_searcher_ImageSearcher_searchNative(
    JNIEnv* env, jclass thiz, jlong native_handle, jlong frame_buffer_handle,
    jintArray jroi) {
  auto* searcher = reinterpret_cast<ImageSearcher*>(native_handle);
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

  auto results_or = searcher->Search(*frame_buffer, roi);
  if (results_or.ok()) {
    return ConvertToSearchResults(env, results_or.value());
  } else {
    ThrowException(
        env, GetExceptionClassNameForStatusCode(results_or.status().code()),
        "Error occurred when searching the image: %s",
        results_or.status().message().data());
    return nullptr;
  }
}
