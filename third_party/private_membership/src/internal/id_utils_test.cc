// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "third_party/private_membership/src/internal/id_utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace private_membership {
namespace {

TEST(IdUtilsTest, PadTest) {
  std::string in = "GOODBYE";
  EXPECT_EQ(PadOrTruncate(in, in.size() + 1), in + "0");
}

TEST(IdUtilsTest, TruncateTest) {
  std::string in = "HELLO";
  EXPECT_EQ(PadOrTruncate(in, in.size() - 1), in.substr(0, in.size() - 1));
}

TEST(IdUtils, NoOpTest) {
  std::string in = "TEST";
  EXPECT_EQ(PadOrTruncate(in, in.size()), in);
}

}  // namespace
}  // namespace private_membership
