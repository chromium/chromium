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

#include "tensorflow_lite_support/java/src/native/task/vision/jni_utils.h"

#include "absl/strings/str_cat.h"
#include "tensorflow_lite_support/cc/utils/jni_utils.h"

namespace tflite {
namespace task {
namespace vision {

using ::tflite::support::utils::kAssertionError;
using ::tflite::support::utils::ThrowException;

constexpr char kCategoryClassName[] =
    "org/tensorflow/lite/support/label/Category";
constexpr char kStringClassName[] = "Ljava/lang/String;";
constexpr char kEmptyString[] = "";

jobject ConvertToCategory(JNIEnv* env, const Class& classification) {
  // jclass and init of Category.
  jclass category_class = env->FindClass(kCategoryClassName);
  jmethodID category_create = env->GetStaticMethodID(
      category_class, "create",
      absl::StrCat("(", kStringClassName, kStringClassName, "F)L",
                   kCategoryClassName, ";")
          .c_str());

  std::string label_string = classification.has_class_name()
                                 ? classification.class_name()
                                 : kEmptyString;
  jstring label = env->NewStringUTF(label_string.c_str());
  std::string display_name_string = classification.has_display_name()
                                        ? classification.display_name()
                                        : kEmptyString;
  jstring display_name = env->NewStringUTF(display_name_string.c_str());
  jobject jcategory =
      env->CallStaticObjectMethod(category_class, category_create, label,
                                  display_name, classification.score());
  return jcategory;
}

FrameBuffer::Orientation ConvertToFrameBufferOrientation(JNIEnv* env,
                                                         jint jorientation) {
  switch (jorientation) {
    case 0:
      return FrameBuffer::Orientation::kTopLeft;
    case 1:
      return FrameBuffer::Orientation::kTopRight;
    case 2:
      return FrameBuffer::Orientation::kBottomRight;
    case 3:
      return FrameBuffer::Orientation::kBottomLeft;
    case 4:
      return FrameBuffer::Orientation::kLeftTop;
    case 5:
      return FrameBuffer::Orientation::kRightTop;
    case 6:
      return FrameBuffer::Orientation::kRightBottom;
    case 7:
      return FrameBuffer::Orientation::kLeftBottom;
  }
  // Should never happen.
  ThrowException(env, kAssertionError,
                 "The FrameBuffer Orientation type is unsupported: %d",
                 jorientation);
  return FrameBuffer::Orientation::kTopLeft;
}

}  // namespace vision
}  // namespace task
}  // namespace tflite
