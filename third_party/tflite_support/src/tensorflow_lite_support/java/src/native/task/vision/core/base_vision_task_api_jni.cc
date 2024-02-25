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

#include "tensorflow_lite_support/cc/task/vision/core/frame_buffer.h"
#include "tensorflow_lite_support/cc/utils/jni_utils.h"
#include "tensorflow_lite_support/java/src/native/task/vision/jni_utils.h"

namespace {

using ::tflite::support::utils::GetExceptionClassNameForStatusCode;
using ::tflite::support::utils::kInvalidPointer;
using ::tflite::support::utils::ThrowException;
using ::tflite::task::vision::CreateFrameBufferFromByteBuffer;
using ::tflite::task::vision::CreateFrameBufferFromBytes;
using ::tflite::task::vision::CreateFrameBufferFromYuvPlanes;
using ::tflite::task::vision::FrameBuffer;

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_vision_core_BaseVisionTaskApi_createFrameBufferFromByteBuffer(
    JNIEnv* env, jclass thiz, jobject jimage_byte_buffer, jint width,
    jint height, jint jorientation, jint jcolor_space_type) {
  auto frame_buffer_or = CreateFrameBufferFromByteBuffer(
      env, jimage_byte_buffer, width, height, jorientation, jcolor_space_type);
  if (frame_buffer_or.ok()) {
    return reinterpret_cast<jlong>(frame_buffer_or->release());
  } else {
    ThrowException(
        env,
        GetExceptionClassNameForStatusCode(frame_buffer_or.status().code()),
        "Error occurred when creating FrameBuffer: %s",
        frame_buffer_or.status().message().data());
    return kInvalidPointer;
  }
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_vision_core_BaseVisionTaskApi_createFrameBufferFromBytes(
    JNIEnv* env, jclass thiz, jbyteArray jimage_bytes, jint width, jint height,
    jint jorientation, jint jcolor_space_type, jlongArray jbyte_array_handle) {
  auto frame_buffer_or =
      CreateFrameBufferFromBytes(env, jimage_bytes, width, height, jorientation,
                                 jcolor_space_type, jbyte_array_handle);
  if (frame_buffer_or.ok()) {
    return reinterpret_cast<jlong>(frame_buffer_or->release());
  } else {
    ThrowException(
        env,
        GetExceptionClassNameForStatusCode(frame_buffer_or.status().code()),
        "Error occurred when creating FrameBuffer: %s",
        frame_buffer_or.status().message().data());
    return kInvalidPointer;
  }
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_task_vision_core_BaseVisionTaskApi_createFrameBufferFromPlanes(
    JNIEnv* env, jclass thiz, jobject jy_plane, jobject ju_plane,
    jobject jv_plane, jint width, jint height, jint row_stride_y,
    jint row_stride_uv, jint pixel_stride_uv, jint orientation) {
  auto frame_buffer_or = CreateFrameBufferFromYuvPlanes(
      env, jy_plane, ju_plane, jv_plane, width, height, row_stride_y,
      row_stride_uv, pixel_stride_uv, orientation);
  if (frame_buffer_or.ok()) {
    return reinterpret_cast<jlong>(frame_buffer_or->release());
  } else {
    ThrowException(
        env,
        GetExceptionClassNameForStatusCode(frame_buffer_or.status().code()),
        "Error occurred when creating FrameBuffer: %s",
        frame_buffer_or.status().message().data());
    return kInvalidPointer;
  }
}

extern "C" JNIEXPORT void JNICALL
Java_org_tensorflow_lite_task_vision_core_BaseVisionTaskApi_deleteFrameBuffer(
    JNIEnv* env, jobject thiz, jlong frame_buffer_handle,
    jlong byte_array_handle, jbyteArray jbyte_array) {
  delete reinterpret_cast<FrameBuffer*>(frame_buffer_handle);
  jbyte* bytes_ptr = reinterpret_cast<jbyte*>(byte_array_handle);
  if (bytes_ptr != NULL) {
    env->ReleaseByteArrayElements(jbyte_array, bytes_ptr, /*mode=*/0);
  }
}

}  // namespace
