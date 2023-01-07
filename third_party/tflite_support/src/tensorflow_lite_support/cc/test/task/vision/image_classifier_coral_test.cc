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

#include <memory>

#include "absl/status/status.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/port/configuration_proto_inc.h"
#include "tensorflow_lite_support/cc/port/gmock.h"
#include "tensorflow_lite_support/cc/port/gtest.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/port/status_matchers.h"
#include "tensorflow_lite_support/cc/task/vision/core/frame_buffer.h"
#include "tensorflow_lite_support/cc/task/vision/image_classifier.h"
#include "tensorflow_lite_support/cc/task/vision/proto/classifications_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/proto/image_classifier_options_proto_inc.h"
#include "tensorflow_lite_support/cc/task/vision/utils/frame_buffer_common_utils.h"
#include "tensorflow_lite_support/cc/task/vision/utils/image_utils.h"

namespace tflite {
namespace task {
namespace vision {
namespace {

using ::tflite::support::StatusOr;

constexpr char kEdgeTpuModelFilePath[] =
    "tensorflow_lite_support/acceleration/configuration/testdata/"
    "mobilenet_v1_1.0_224_quant_edgetpu.tflite";
constexpr char kRegularModelFilePath[] =
    "tensorflow_lite_support/acceleration/configuration/testdata/"
    "mobilenet_v1_1.0_224_quant.tflite";
constexpr char kImagePath[] =
    "tensorflow_lite_support/acceleration/configuration/testdata/"
    "burger.jpg";

using ClassifyTest = testing::TestWithParam<std::string>;

}  // namespace
}  // namespace vision
}  // namespace task
}  // namespace tflite
