// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "statusor.h"

#include <glog/logging.h>
#include "absl/status/status.h"

namespace rlwe {
namespace internal {

static const char* kInvalidStatusCtorArgMessage =
    "Status::OK is not a valid constructor argument to StatusOr<T>";
static const char* kNullObjectCtorArgMessage =
    "NULL is not a valid constructor argument to StatusOr<T*>";

absl::Status StatusOrHelper::HandleInvalidStatusCtorArg() {
  LOG(DFATAL) << kInvalidStatusCtorArgMessage;
  return absl::InternalError(kInvalidStatusCtorArgMessage);
}

absl::Status StatusOrHelper::HandleNullObjectCtorArg() {
  LOG(DFATAL) << kNullObjectCtorArgMessage;
  return absl::InternalError(kNullObjectCtorArgMessage);
}

void StatusOrHelper::Crash(const absl::Status& status) {
  LOG(FATAL) << "Attempting to fetch value instead of handling error "
             << status.ToString().c_str();
}

}  // namespace internal
}  // namespace rlwe
