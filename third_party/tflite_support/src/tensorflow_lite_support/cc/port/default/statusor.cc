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
// This file is forked from absl.

#include "tensorflow_lite_support/cc/port/default/statusor.h"

#include <utility>

#include "absl/strings/str_cat.h"
#include "base/logging.h"

namespace tflite {
namespace support {

BadStatusOrAccess::BadStatusOrAccess(absl::Status status)
    : status_(std::move(status)) {}

BadStatusOrAccess::~BadStatusOrAccess() = default;

const char* BadStatusOrAccess::what() const noexcept {
  return "Bad StatusOr access";
}

const absl::Status& BadStatusOrAccess::status() const {
  return status_;
}

namespace internal_statusor {

void Helper::HandleInvalidStatusCtorArg(absl::Status* status) {
  const char* kMessage =
      "An OK status is not a valid constructor argument to StatusOr<T>";
  LOG(DFATAL) << kMessage;
  // In optimized builds, we will fall back to ::util::error::INTERNAL.
  *status = absl::InternalError(kMessage);
}

void Helper::Crash(const absl::Status& status) {
  LOG(FATAL) << "Attempting to fetch value instead of handling error "
             << status;
  _Exit(1);
}

void ThrowBadStatusOrAccess(absl::Status status) {
#ifdef ABSL_HAVE_EXCEPTIONS
  throw BadStatusOrAccess(std::move(status));
#else
  LOG(FATAL) << "Attempting to fetch value instead of handling error "
             << status;
#endif
}

}  // namespace internal_statusor
}  // namespace support
}  // namespace tflite
