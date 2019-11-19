/*
 * Copyright 2019 Google Inc.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "third_party/private-join-and-compute/src/util/statusor.h"

#include "third_party/private-join-and-compute/src/chromium_patch.h"
#include "third_party/private-join-and-compute/src/util/status.h"

namespace private_join_and_compute {
namespace internal {

static const char* kInvalidStatusCtorArgMessage =
    "Status::OK is not a valid constructor argument to StatusOr<T>";
static const char* kNullObjectCtorArgMessage =
    "nullptr is not a valid constructor argument to StatusOr<T*>";

Status StatusOrHelper::HandleInvalidStatusCtorArg() {
  LOG(DFATAL) << kInvalidStatusCtorArgMessage;
  // In optimized builds, we will fall back to private_join_and_compute::StatusCode::kInternal.
  return Status(::private_join_and_compute::StatusCode::kInternal,
                kInvalidStatusCtorArgMessage);
}

Status StatusOrHelper::HandleNullObjectCtorArg() {
  LOG(DFATAL) << kNullObjectCtorArgMessage;
  // In optimized builds, we will fall back to
  // ::private_join_and_compute::StatusCode::kInternal.
  return Status(::private_join_and_compute::StatusCode::kInternal, kNullObjectCtorArgMessage);
}

void StatusOrHelper::Crash(const Status& status) {
  LOG(FATAL) << "Attempting to fetch value instead of handling error "
             << status;
}

}  // namespace internal
}  // namespace private_join_and_compute
