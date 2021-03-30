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

#ifndef TENSORFLOW_LITE_SUPPORT_JAVA_SRC_NATIVE_TASK_VISION_JNI_UTILS_H_
#define TENSORFLOW_LITE_SUPPORT_JAVA_SRC_NATIVE_TASK_VISION_JNI_UTILS_H_

#include <jni.h>

#include "tensorflow_lite_support/cc/task/vision/core/frame_buffer.h"
#include "tensorflow_lite_support/cc/task/vision/proto/class_proto_inc.h"

namespace tflite {
namespace task {
namespace vision {

// Creates a Java Category object based on Class.
jobject ConvertToCategory(JNIEnv* env, const Class& classification);

FrameBuffer::Orientation ConvertToFrameBufferOrientation(JNIEnv* env,
                                                         jint jorientation);

}  // namespace vision
}  // namespace task
}  // namespace tflite

#endif  // TENSORFLOW_LITE_SUPPORT_JAVA_SRC_NATIVE_TASK_VISION_JNI_UTILS_H_
