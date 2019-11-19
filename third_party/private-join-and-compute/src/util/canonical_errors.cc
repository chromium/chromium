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

#include "third_party/private-join-and-compute/src/util/canonical_errors.h"

#include <string>

#include "third_party/private-join-and-compute/src/util/status.h"

namespace private_join_and_compute {

Status InternalError(const std::string& message) {
  return Status(private_join_and_compute::StatusCode::kInternal, message);
}

Status InvalidArgumentError(const std::string& message) {
  return Status(private_join_and_compute::StatusCode::kInvalidArgument, message);
}

bool IsInternal(const Status& status) {
  return status.code() == private_join_and_compute::StatusCode::kInternal;
}

bool IsInvalidArgument(const Status& status) {
  return status.code() == private_join_and_compute::StatusCode::kInvalidArgument;
}

}  // namespace private_join_and_compute
