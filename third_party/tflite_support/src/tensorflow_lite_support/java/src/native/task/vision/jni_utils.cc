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

#include "absl/strings/str_cat.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/task/vision/core/frame_buffer.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_common_utils.h"
#include "tensorflow_lite_support/cc/utils/jni_utils.h"

namespace tflite {
namespace task {
namespace vision {

using ::tflite::support::StatusOr;
using ::tflite::support::utils::GetMappedFileBuffer;
using ::tflite::support::utils::kIllegalStateException;
using ::tflite::support::utils::ThrowException;
using ::tflite::task::vision::CreateFromRawBuffer;

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

FrameBuffer::Format ConvertToFrameBufferFormat(JNIEnv* env,
                                               jint jcolor_space_type) {
  switch (jcolor_space_type) {
    case 0:
      return FrameBuffer::Format::kRGB;
    case 1:
      return FrameBuffer::Format::kGRAY;
    case 2:
      return FrameBuffer::Format::kNV12;
    case 3:
      return FrameBuffer::Format::kNV21;
    case 4:
      return FrameBuffer::Format::kYV12;
    case 5:
      return FrameBuffer::Format::kYV21;
    default:
      break;
  }
  // Should never happen.
  ThrowException(env, kIllegalStateException,
                 "The color space type is unsupported: %d", jcolor_space_type);
  return FrameBuffer::Format::kRGB;
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
  ThrowException(env, kIllegalStateException,
                 "The FrameBuffer Orientation type is unsupported: %d",
                 jorientation);
  return FrameBuffer::Orientation::kTopLeft;
}

// TODO(b/180051417): remove the code, once FrameBuffer can digest YUV buffers
// without format.
// Theoretically, when using CreateFromYuvRawBuffer, "format" can always be set
// to YV12 (or YV21, they are identical). However, prefer to set format to NV12
// or NV21 whenever it's applicable, because NV12 and NV21 is better optimized
// in performance than YV12 or YV21.
StatusOr<FrameBuffer::Format> GetYUVImageFormat(const uint8* u_buffer,
                                                const uint8* v_buffer,
                                                int uv_pixel_stride) {
  intptr_t u = reinterpret_cast<intptr_t>(u_buffer);
  intptr_t v = reinterpret_cast<intptr_t>(v_buffer);
  if ((std::abs(u - v) == 1) && (uv_pixel_stride == 2)) {
    if (u_buffer > v_buffer) {
      return FrameBuffer::Format::kNV21;
    } else {
      return FrameBuffer::Format::kNV12;
    }
  }
  return FrameBuffer::Format::kYV12;
}

StatusOr<std::unique_ptr<FrameBuffer>> CreateFrameBufferFromByteBuffer(
    JNIEnv* env, jobject jimage_byte_buffer, jint width, jint height,
    jint jorientation, jint jcolor_space_type) {
  absl::string_view image = GetMappedFileBuffer(env, jimage_byte_buffer);
  return CreateFromRawBuffer(
      reinterpret_cast<const uint8*>(image.data()),
      FrameBuffer::Dimension{width, height},
      ConvertToFrameBufferFormat(env, jcolor_space_type),
      ConvertToFrameBufferOrientation(env, jorientation));
}

StatusOr<std::unique_ptr<FrameBuffer>> CreateFrameBufferFromBytes(
    JNIEnv* env, jbyteArray jimage_bytes, jint width, jint height,
    jint jorientation, jint jcolor_space_type, jlongArray jbyte_array_handle) {
  jbyte* jimage_ptr = env->GetByteArrayElements(jimage_bytes, NULL);
  // Free jimage_ptr together with frame_buffer after inference is finished.
  jlong jimage_ptr_handle = reinterpret_cast<jlong>(jimage_ptr);
  // jbyte_array_handle has only one element, which is a holder for jimage_ptr.
  env->SetLongArrayRegion(jbyte_array_handle, 0, 1, &jimage_ptr_handle);

  if (jimage_ptr == NULL) {
    ThrowException(env, kIllegalStateException,
                   "Error occurred when reading image data from byte array.");
    return nullptr;
  }

  return CreateFromRawBuffer(
      reinterpret_cast<const uint8*>(jimage_ptr),
      FrameBuffer::Dimension{width, height},
      ConvertToFrameBufferFormat(env, jcolor_space_type),
      ConvertToFrameBufferOrientation(env, jorientation));
}

StatusOr<std::unique_ptr<FrameBuffer>> CreateFrameBufferFromYuvPlanes(
    JNIEnv* env, jobject jy_plane, jobject ju_plane, jobject jv_plane,
    jint width, jint height, jint row_stride_y, jint row_stride_uv,
    jint pixel_stride_uv, jint jorientation) {
  const uint8* y_plane =
      reinterpret_cast<const uint8*>(GetMappedFileBuffer(env, jy_plane).data());
  const uint8* u_plane =
      reinterpret_cast<const uint8*>(GetMappedFileBuffer(env, ju_plane).data());
  const uint8* v_plane =
      reinterpret_cast<const uint8*>(GetMappedFileBuffer(env, jv_plane).data());

  FrameBuffer::Format format;
  TFLITE_ASSIGN_OR_RETURN(format,
                   GetYUVImageFormat(u_plane, v_plane, pixel_stride_uv));

  return CreateFromYuvRawBuffer(
      y_plane, u_plane, v_plane, format, FrameBuffer::Dimension{width, height},
      row_stride_y, row_stride_uv, pixel_stride_uv,
      ConvertToFrameBufferOrientation(env, jorientation));
}

}  // namespace vision
}  // namespace task
}  // namespace tflite
