// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef MALDOCA_BASE_STATUS_H_
#define MALDOCA_BASE_STATUS_H_

#ifdef MALDOCA_CHROME

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "maldoca/base/logging.h"
#include "maldoca/base/status_proto.pb.h"

#define MALDOCA_CHECK_OK(val) CHECK((val).ok())
#define MALDOCA_DCHECK_OK(val) DCHECK((val).ok())

namespace maldoca {
// TBD if this is a good value.
constexpr char kMaldocaStatusType[] =
    "type.googleapis.com/google.maldoca.MaldocaErrorCode";

// Decorate the status with given code as a payload.
inline absl::Status& StatusWithErrorCode(MaldocaErrorCode code,
                                         absl::Status& status) {
  CHECK(MaldocaErrorCode_IsValid(code));  // Must pass in valid code
  // Using string for now
  status.SetPayload(kMaldocaStatusType,
                    absl::Cord(::maldoca::MaldocaErrorCode_Name(code)));
  return status;
}

// For convenience, add more as needed.
inline absl::Status ResourceExhaustedError(absl::string_view message,
                                           MaldocaErrorCode code) {
  auto status = absl::ResourceExhaustedError(message);
  return StatusWithErrorCode(code, status);
}

inline absl::Status OutOfRangeError(absl::string_view message,
                                    MaldocaErrorCode code) {
  auto status = absl::OutOfRangeError(message);
  return StatusWithErrorCode(code, status);
}

inline absl::Status InternalError(absl::string_view message,
                                  MaldocaErrorCode code) {
  auto status = absl::InternalError(message);
  StatusWithErrorCode(code, status);
  return status;
}

inline absl::Status AbortedError(absl::string_view message,
                                 MaldocaErrorCode code) {
  auto status = absl::AbortedError(message);
  StatusWithErrorCode(code, status);
  return status;
}

inline absl::Status UnimplementedError(absl::string_view message,
                                       MaldocaErrorCode code) {
  auto status = absl::UnimplementedError(message);
  StatusWithErrorCode(code, status);
  return status;
}
}  // namespace maldoca

#else  // MALDOCA_CHROME
#include "zetasql/base/statusor.h"

namespace maldoca {
using ::zetasql_base::StatusOr;
}  // namespace maldoca

#endif  // MALDOCA_CHROME
#endif  // MALDOCA_BASE_STATUS_H_
