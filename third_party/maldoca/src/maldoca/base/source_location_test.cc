// Copyright 2021 Google LLC
// Copyright 2018 ZetaSQL Authors
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

#include "maldoca/base/source_location.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using ::maldoca::SourceLocation;
using ::testing::EndsWith;

TEST(SourceLocationTest, CopyConstructionWorks) {
  constexpr SourceLocation location = MALDOCA_LOC;

  EXPECT_EQ(location.line(), __LINE__ - 2);
  EXPECT_THAT(location.file_name(), EndsWith("source_location_test.cc"));
}

TEST(SourceLocationTest, CopyAssignmentWorks) {
  SourceLocation location;
  location = MALDOCA_LOC;

  EXPECT_EQ(location.line(), __LINE__ - 2);
  EXPECT_THAT(location.file_name(), EndsWith("source_location_test.cc"));
}

SourceLocation Echo(const SourceLocation& location) { return location; }

TEST(SourceLocationTest, ExpectedUsageWorks) {
  SourceLocation location = Echo(MALDOCA_LOC);

  EXPECT_EQ(location.line(), __LINE__ - 2);
  EXPECT_THAT(location.file_name(), EndsWith("source_location_test.cc"));
}

}  // namespace