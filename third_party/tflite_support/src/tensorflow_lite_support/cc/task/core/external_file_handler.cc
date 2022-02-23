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

#include "tensorflow_lite_support/cc/task/core/external_file_handler.h"

#include <stddef.h>
#include <memory>
#include <string>

#include "absl/memory/memory.h"       // from @com_google_absl
#include "absl/strings/str_format.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/common.h"
#include "tensorflow_lite_support/cc/port/status_macros.h"
#include "tensorflow_lite_support/cc/port/statusor.h"

namespace tflite {
namespace task {
namespace core {

using ::absl::StatusCode;
using ::tflite::support::CreateStatusWithPayload;
using ::tflite::support::StatusOr;
using ::tflite::support::TfLiteSupportStatus;

/* static */
StatusOr<std::unique_ptr<ExternalFileHandler>>
ExternalFileHandler::CreateFromExternalFile(const ExternalFile* external_file) {
  // Use absl::WrapUnique() to call private constructor:
  // https://abseil.io/tips/126.
  std::unique_ptr<ExternalFileHandler> handler =
      absl::WrapUnique(new ExternalFileHandler(external_file));

  RETURN_IF_ERROR(handler->MapExternalFile());

  return handler;
}

absl::Status ExternalFileHandler::MapExternalFile() {
  if (!external_file_.file_content().empty()) {
    return absl::OkStatus();
  }

  return CreateStatusWithPayload(StatusCode::kInvalidArgument,
                                 "ExternalFile must specify 'file_content' "
                                 "to be compatible with Chromium.",
                                 TfLiteSupportStatus::kInvalidArgumentError);
}

absl::string_view ExternalFileHandler::GetFileContent() {
  return external_file_.file_content();
}

ExternalFileHandler::~ExternalFileHandler() = default;

}  // namespace core
}  // namespace task
}  // namespace tflite
