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

#include "absl/strings/cord.h"  // from @com_google_absl
#include "absl/strings/str_cat.h"  // from @com_google_absl

namespace tflite {
namespace support {

absl::Status CreateStatusWithPayload(absl::StatusCode canonical_code,
                                     absl::string_view message,
                                     TfLiteSupportStatus tfls_code) {
  // NOTE: Ignores `message` if the canonical code is ok.
  absl::Status status = absl::Status(canonical_code, message);
  // NOTE: Does nothing if the canonical code is ok.
  status.SetPayload(kTfLiteSupportPayload, absl::Cord(absl::StrCat(tfls_code)));
  return status;
}

}  // namespace support
}  // namespace tflite
