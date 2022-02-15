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

#include "status_macros.h"

#include <sstream>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/status/status.h"
#include "statusor.h"

namespace rlwe {
namespace {

TEST(StatusMacrosTest, TestAssignOrReturn) {
  StatusOr<StatusOr<int>> a(StatusOr<int>(2));
  auto f = [&]() -> absl::Status {
    RLWE_ASSIGN_OR_RETURN(StatusOr<int> status_or_a, a.value());
    EXPECT_EQ(2, status_or_a.value());
    return absl::OkStatus();
  };
  auto status = f();
  EXPECT_TRUE(status.ok()) << status;
}

TEST(StatusMacrosTest, TestAssignOrReturnFails) {
  auto a = []() -> StatusOr<int> { return absl::InternalError("error"); };
  auto f = [&]() -> absl::Status {
    RLWE_ASSIGN_OR_RETURN(auto result, a());
    result++;
    return absl::OkStatus();
  };
  auto status = f();
  EXPECT_EQ(absl::StatusCode::kInternal, status.code());
  EXPECT_EQ("error", status.message());
}

}  // namespace
}  // namespace rlwe
