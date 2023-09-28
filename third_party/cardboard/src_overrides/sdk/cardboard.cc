/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "third_party/cardboard/src/sdk/include/cardboard.h"

#include <cmath>

#include "third_party/cardboard/src/sdk/distortion_renderer.h"
#include "third_party/cardboard/src/sdk/head_tracker.h"
#include "third_party/cardboard/src/sdk/lens_distortion.h"
#include "third_party/cardboard/src/sdk/qr_code.h"
#include "third_party/cardboard/src/sdk/qrcode/cardboard_v1/cardboard_v1.h"
#include "third_party/cardboard/src/sdk/screen_params.h"
#include "third_party/cardboard/src/sdk/util/is_arg_null.h"
#include "third_party/cardboard/src/sdk/util/is_initialized.h"
#include "third_party/cardboard/src/sdk/util/logging.h"
#ifdef __ANDROID__
#include "third_party/cardboard/src/sdk/device_params/android/device_params.h"
#include "third_party/cardboard/src/sdk/jni_utils/android/jni_utils.h"
#endif

// TODO(b/134142617): Revisit struct/class hierarchy.
struct CardboardLensDistortion : cardboard::LensDistortion {};
struct CardboardDistortionRenderer : cardboard::DistortionRenderer {};
struct CardboardHeadTracker : cardboard::HeadTracker {};

namespace {

// Return default (identity) matrix.
void GetDefaultMatrix(float* matrix) {
  if (matrix != nullptr) {
    for (int i = 0; i < 4; ++i) {
      for (int j = 0; j < 4; ++j) {
        matrix[i * 4 + j] = (i == j) ? 1.0f : 0.0f;
      }
    }
  }
}

// Return default (all angles equal to 45 degrees) field of view.
void GetDefaultEyeFieldOfView(float* field_of_view) {
  if (field_of_view != nullptr) {
    float default_angle = 45.0f * M_PI / 180.0f;
    for (int i = 0; i < 4; ++i) {
      field_of_view[i] = default_angle;
    }
  }
}

// Return default (empty) distortion mesh.
void GetDefaultDistortionMesh(CardboardMesh* mesh) {
  if (mesh != nullptr) {
    mesh->indices = nullptr;
    mesh->n_indices = 0;
    mesh->vertices = nullptr;
    mesh->uvs = nullptr;
    mesh->n_vertices = 0;
  }
}

// Return default (empty) encoded device params.
void GetDefaultEncodedDeviceParams(uint8_t** encoded_device_params, int* size) {
  if (encoded_device_params != nullptr) {
    *encoded_device_params = nullptr;
  }
  if (size != nullptr) {
    *size = 0;
  }
}

// Return default (zero) position.
void GetDefaultPosition(float* position) {
  if (position != nullptr) {
    position[0] = 0.0f;
    position[1] = 0.0f;
    position[2] = 0.0f;
  }
}

// Return default (identity quaternion) orientation.
void GetDefaultOrientation(float* orientation) {
  if (orientation != nullptr) {
    orientation[0] = 0.0f;
    orientation[1] = 0.0f;
    orientation[2] = 0.0f;
    orientation[3] = 1.0f;
  }
}

}  // anonymous namespace

extern "C" {

#ifdef __ANDROID__
void Cardboard_initializeAndroid(JavaVM* vm, jobject context) {
  if (CARDBOARD_IS_ARG_NULL(vm) || CARDBOARD_IS_ARG_NULL(context)) {
    return;
  }
  JNIEnv* env;
  vm->GetEnv((void**)&env, JNI_VERSION_1_6);
  jobject global_context = env->NewGlobalRef(context);

  cardboard::jni::initializeAndroid(vm, global_context);
  cardboard::qrcode::initializeAndroid(vm, global_context);
  cardboard::screen_params::initializeAndroid(vm, global_context);
  cardboard::DeviceParams::initializeAndroid(vm, global_context);

  cardboard::util::SetIsInitialized();
}
#endif

CardboardLensDistortion* CardboardLensDistortion_create(
    const uint8_t* encoded_device_params,
    int size,
    int display_width,
    int display_height) {
  if (CARDBOARD_IS_NOT_INITIALIZED() ||
      CARDBOARD_IS_ARG_NULL(encoded_device_params)) {
    return nullptr;
  }
  return reinterpret_cast<CardboardLensDistortion*>(
      new cardboard::LensDistortion(encoded_device_params, size, display_width,
                                    display_height));
}

void CardboardLensDistortion_destroy(CardboardLensDistortion* lens_distortion) {
  if (CARDBOARD_IS_NOT_INITIALIZED() ||
      CARDBOARD_IS_ARG_NULL(lens_distortion)) {
    return;
  }
  delete lens_distortion;
}

void CardboardLensDistortion_getEyeFromHeadMatrix(
    CardboardLensDistortion* lens_distortion,
    CardboardEye eye,
    float* eye_from_head_matrix) {
  if (CARDBOARD_IS_NOT_INITIALIZED() ||
      CARDBOARD_IS_ARG_NULL(lens_distortion) ||
      CARDBOARD_IS_ARG_NULL(eye_from_head_matrix)) {
    GetDefaultMatrix(eye_from_head_matrix);
    return;
  }
  static_cast<cardboard::LensDistortion*>(lens_distortion)
      ->GetEyeFromHeadMatrix(eye, eye_from_head_matrix);
}

void CardboardLensDistortion_getProjectionMatrix(
    CardboardLensDistortion* lens_distortion,
    CardboardEye eye,
    float z_near,
    float z_far,
    float* projection_matrix) {
  if (CARDBOARD_IS_NOT_INITIALIZED() ||
      CARDBOARD_IS_ARG_NULL(lens_distortion) ||
      CARDBOARD_IS_ARG_NULL(projection_matrix)) {
    GetDefaultMatrix(projection_matrix);
    return;
  }
  static_cast<cardboard::LensDistortion*>(lens_distortion)
      ->GetEyeProjectionMatrix(eye, z_near, z_far, projection_matrix);
}

void CardboardLensDistortion_getFieldOfView(
    CardboardLensDistortion* lens_distortion,
    CardboardEye eye,
    float* field_of_view) {
  if (CARDBOARD_IS_NOT_INITIALIZED() ||
      CARDBOARD_IS_ARG_NULL(lens_distortion) ||
      CARDBOARD_IS_ARG_NULL(field_of_view)) {
    GetDefaultEyeFieldOfView(field_of_view);
    return;
  }
  static_cast<cardboard::LensDistortion*>(lens_distortion)
      ->GetEyeFieldOfView(eye, field_of_view);
}

void CardboardLensDistortion_getDistortionMesh(
    CardboardLensDistortion* lens_distortion,
    CardboardEye eye,
    CardboardMesh* mesh) {
  if (CARDBOARD_IS_NOT_INITIALIZED() ||
      CARDBOARD_IS_ARG_NULL(lens_distortion) || CARDBOARD_IS_ARG_NULL(mesh)) {
    GetDefaultDistortionMesh(mesh);
    return;
  }
  *mesh = static_cast<cardboard::LensDistortion*>(lens_distortion)
              ->GetDistortionMesh(eye);
}

CardboardUv CardboardLensDistortion_undistortedUvForDistortedUv(
    CardboardLensDistortion* lens_distortion,
    const CardboardUv* distorted_uv,
    CardboardEye eye) {
  if (CARDBOARD_IS_NOT_INITIALIZED() ||
      CARDBOARD_IS_ARG_NULL(lens_distortion) ||
      CARDBOARD_IS_ARG_NULL(distorted_uv)) {
    return CardboardUv{/*.u=*/-1, /*.v=*/-1};
  }

  std::array<float, 2> in = {distorted_uv->u, distorted_uv->v};
  std::array<float, 2> out =
      static_cast<cardboard::LensDistortion*>(lens_distortion)
          ->UndistortedUvForDistortedUv(in, eye);

  CardboardUv ret;
  ret.u = out[0];
  ret.v = out[1];
  return ret;
}

CardboardUv CardboardLensDistortion_distortedUvForUndistortedUv(
    CardboardLensDistortion* lens_distortion,
    const CardboardUv* undistorted_uv,
    CardboardEye eye) {
  if (CARDBOARD_IS_NOT_INITIALIZED() ||
      CARDBOARD_IS_ARG_NULL(lens_distortion) ||
      CARDBOARD_IS_ARG_NULL(undistorted_uv)) {
    return CardboardUv{/*.u=*/-1, /*.v=*/-1};
  }

  std::array<float, 2> in = {undistorted_uv->u, undistorted_uv->v};
  std::array<float, 2> out =
      static_cast<cardboard::LensDistortion*>(lens_distortion)
          ->DistortedUvForUndistortedUv(in, eye);

  CardboardUv ret;
  ret.u = out[0];
  ret.v = out[1];
  return ret;
}

void CardboardDistortionRenderer_destroy(
    CardboardDistortionRenderer* renderer) {
  if (CARDBOARD_IS_NOT_INITIALIZED() || CARDBOARD_IS_ARG_NULL(renderer)) {
    return;
  }
  delete renderer;
}

void CardboardDistortionRenderer_setMesh(CardboardDistortionRenderer* renderer,
                                         const CardboardMesh* mesh,
                                         CardboardEye eye) {
  if (CARDBOARD_IS_NOT_INITIALIZED() || CARDBOARD_IS_ARG_NULL(renderer) ||
      CARDBOARD_IS_ARG_NULL(mesh)) {
    return;
  }
  static_cast<cardboard::DistortionRenderer*>(renderer)->SetMesh(mesh, eye);
}

void CardboardDistortionRenderer_renderEyeToDisplay(
    CardboardDistortionRenderer* renderer,
    uint64_t target,
    int x,
    int y,
    int width,
    int height,
    const CardboardEyeTextureDescription* left_eye,
    const CardboardEyeTextureDescription* right_eye) {
  if (CARDBOARD_IS_NOT_INITIALIZED() || CARDBOARD_IS_ARG_NULL(renderer) ||
      CARDBOARD_IS_ARG_NULL(left_eye) || CARDBOARD_IS_ARG_NULL(right_eye)) {
    return;
  }
  static_cast<cardboard::DistortionRenderer*>(renderer)->RenderEyeToDisplay(
      target, x, y, width, height, left_eye, right_eye);
}

CardboardHeadTracker* CardboardHeadTracker_create() {
  if (CARDBOARD_IS_NOT_INITIALIZED()) {
    return nullptr;
  }
  return reinterpret_cast<CardboardHeadTracker*>(new cardboard::HeadTracker());
}

void CardboardHeadTracker_destroy(CardboardHeadTracker* head_tracker) {
  if (CARDBOARD_IS_NOT_INITIALIZED() || CARDBOARD_IS_ARG_NULL(head_tracker)) {
    return;
  }
  delete head_tracker;
}

void CardboardHeadTracker_pause(CardboardHeadTracker* head_tracker) {
  if (CARDBOARD_IS_NOT_INITIALIZED() || CARDBOARD_IS_ARG_NULL(head_tracker)) {
    return;
  }
  static_cast<cardboard::HeadTracker*>(head_tracker)->Pause();
}

void CardboardHeadTracker_resume(CardboardHeadTracker* head_tracker) {
  if (CARDBOARD_IS_NOT_INITIALIZED() || CARDBOARD_IS_ARG_NULL(head_tracker)) {
    return;
  }
  static_cast<cardboard::HeadTracker*>(head_tracker)->Resume();
}

void CardboardHeadTracker_getPose(
    CardboardHeadTracker* head_tracker,
    int64_t timestamp_ns,
    CardboardViewportOrientation viewport_orientation,
    float* position,
    float* orientation) {
  if (CARDBOARD_IS_NOT_INITIALIZED() || CARDBOARD_IS_ARG_NULL(head_tracker) ||
      CARDBOARD_IS_ARG_NULL(position) || CARDBOARD_IS_ARG_NULL(orientation)) {
    GetDefaultPosition(position);
    GetDefaultOrientation(orientation);
    return;
  }
  std::array<float, 3> out_position;
  std::array<float, 4> out_orientation;
  static_cast<cardboard::HeadTracker*>(head_tracker)
      ->GetPose(timestamp_ns, viewport_orientation, out_position,
                out_orientation);
  std::memcpy(position, &out_position[0], 3 * sizeof(float));
  std::memcpy(orientation, &out_orientation[0], 4 * sizeof(float));
}

void CardboardHeadTracker_recenter(CardboardHeadTracker* head_tracker) {
  if (CARDBOARD_IS_NOT_INITIALIZED() || CARDBOARD_IS_ARG_NULL(head_tracker)) {
    return;
  }
  static_cast<cardboard::HeadTracker*>(head_tracker)->Recenter();
}

void CardboardQrCode_getSavedDeviceParams(uint8_t** encoded_device_params,
                                          int* size) {
  if (CARDBOARD_IS_NOT_INITIALIZED() ||
      CARDBOARD_IS_ARG_NULL(encoded_device_params) ||
      CARDBOARD_IS_ARG_NULL(size)) {
    GetDefaultEncodedDeviceParams(encoded_device_params, size);
    return;
  }
  std::vector<uint8_t> device_params =
      cardboard::qrcode::getCurrentSavedDeviceParams();
  if (device_params.empty()) {
    CARDBOARD_LOGD("No device parameters currently saved.");
    *size = 0;
    *encoded_device_params = nullptr;
    return;
  }
  *size = static_cast<int>(device_params.size());
  *encoded_device_params = new uint8_t[*size];
  memcpy(*encoded_device_params, &device_params[0], *size);
}

void CardboardQrCode_destroy(const uint8_t* encoded_device_params) {
  if (CARDBOARD_IS_NOT_INITIALIZED() ||
      CARDBOARD_IS_ARG_NULL(encoded_device_params)) {
    return;
  }
  delete[] encoded_device_params;
}

void CardboardQrCode_saveDeviceParams(const uint8_t* uri, int size) {
  if (CARDBOARD_IS_NOT_INITIALIZED() || CARDBOARD_IS_ARG_NULL(uri)) {
    return;
  }
  if (size <= 0) {
    CARDBOARD_LOGE(
        "[%s : %d] Argument size is not valid. It must be higher than zero.",
        __FILE__, __LINE__);
    return;
  }
  cardboard::qrcode::saveDeviceParams(uri, size);
}

void CardboardQrCode_scanQrCodeAndSaveDeviceParams() {
  if (CARDBOARD_IS_NOT_INITIALIZED()) {
    return;
  }
  cardboard::qrcode::scanQrCodeAndSaveDeviceParams();
}

int CardboardQrCode_getDeviceParamsChangedCount() {
  if (CARDBOARD_IS_NOT_INITIALIZED()) {
    return 0;
  }
  return cardboard::qrcode::getDeviceParamsChangedCount();
}

void CardboardQrCode_getCardboardV1DeviceParams(uint8_t** encoded_device_params,
                                                int* size) {
  if (CARDBOARD_IS_ARG_NULL(encoded_device_params) ||
      CARDBOARD_IS_ARG_NULL(size)) {
    GetDefaultEncodedDeviceParams(encoded_device_params, size);
    return;
  }
  static std::vector<uint8_t> cardboard_v1_device_param =
      cardboard::qrcode::getCardboardV1DeviceParams();
  *encoded_device_params = cardboard_v1_device_param.data();
  *size = static_cast<int>(cardboard_v1_device_param.size());
}

}  // extern "C"
