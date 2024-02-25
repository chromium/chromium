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

#include "tensorflow_lite_support/cc/common.h"

#include "absl/status/status.h"  // from @com_google_absl
#include "absl/strings/cord.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/port/gmock.h"
#include "tensorflow_lite_support/cc/port/gtest.h"

namespace tflite {
namespace support {
namespace {

using testing::Optional;

TEST(CommonTest, CreateStatusWithPayloadWorks) {
  absl::Status status = CreateStatusWithPayload(
      absl::StatusCode::kInvalidArgument, "Bad schema version: BADF",
      TfLiteSupportStatus::kMetadataInvalidSchemaVersionError);
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(status.message(), std::string("Bad schema version: BADF"));
  EXPECT_THAT(status.GetPayload(kTfLiteSupportPayload),
              Optional(absl::Cord("200")));
}

}  // namespace
}  // namespace support
}  // namespace tflite
