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

#include "maldoca/base/status_macros.h"

#include <memory>
#include <string>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "maldoca/base/source_location.h"
#include "maldoca/base/status.h"
#ifndef MALDOCA_CHROME
#include "maldoca/base/status_builder.h"
#endif
#include "maldoca/base/statusor.h"

namespace {

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::HasSubstr;

absl::Status ReturnOk() { return absl::OkStatus(); }

#ifndef MALDOCA_CHROME
maldoca::StatusBuilder ReturnOkBuilder() {
  return maldoca::StatusBuilder(absl::OkStatus(), MALDOCA_LOC);
}

maldoca::StatusBuilder ReturnErrorBuilder(absl::string_view msg) {
  return maldoca::StatusBuilder(absl::Status(absl::StatusCode::kUnknown, msg),
                                MALDOCA_LOC);
}
#endif  // MALDOCA_CHROME

absl::Status ReturnError(absl::string_view msg) {
  return absl::Status(absl::StatusCode::kUnknown, msg);
}

maldoca::StatusOr<int> ReturnStatusOrValue(int v) { return v; }

maldoca::StatusOr<int> ReturnStatusOrError(absl::string_view msg) {
  return absl::Status(absl::StatusCode::kUnknown, msg);
}

maldoca::StatusOr<std::unique_ptr<int>> ReturnStatusOrPtrValue(int v) {
  return absl::make_unique<int>(v);
}

TEST(AssignOrReturn, Works) {
  auto func = []() -> absl::Status {
    MALDOCA_ASSIGN_OR_RETURN(int value1, ReturnStatusOrValue(1));
    EXPECT_EQ(1, value1);
    MALDOCA_ASSIGN_OR_RETURN(const int value2, ReturnStatusOrValue(2));
    EXPECT_EQ(2, value2);
    MALDOCA_ASSIGN_OR_RETURN(const int& value3, ReturnStatusOrValue(3));
    EXPECT_EQ(3, value3);
    MALDOCA_ASSIGN_OR_RETURN(ABSL_ATTRIBUTE_UNUSED int value4,
                             ReturnStatusOrError("EXPECTED"));
    return ReturnError("ERROR");
  };

  EXPECT_THAT(func().message(), Eq("EXPECTED"));
}

TEST(AssignOrReturn, WorksWithAppend) {
  auto fail_test_if_called = []() -> std::string {
    ADD_FAILURE();
    return "FAILURE";
  };
  auto func = [&]() -> absl::Status {
    ABSL_ATTRIBUTE_UNUSED int value;
    MALDOCA_ASSIGN_OR_RETURN(value, ReturnStatusOrValue(1),
                             _ << fail_test_if_called());
    MALDOCA_ASSIGN_OR_RETURN(value, ReturnStatusOrError("EXPECTED A"),
                             _ << "EXPECTED B");
    return ReturnOk();
  };

  EXPECT_THAT(func().message(),
              AllOf(HasSubstr("EXPECTED A"), HasSubstr("EXPECTED B")));
}

TEST(AssignOrReturn, WorksWithAppendIncludingLocals) {
  auto func = [&](const std::string& str) -> absl::Status {
    ABSL_ATTRIBUTE_UNUSED int value;
    MALDOCA_ASSIGN_OR_RETURN(value, ReturnStatusOrError("EXPECTED A"),
                             _ << str);
    return ReturnOk();
  };

  EXPECT_THAT(func("EXPECTED B").message(),
              AllOf(HasSubstr("EXPECTED A"), HasSubstr("EXPECTED B")));
}

TEST(AssignOrReturn, WorksForExistingVariable) {
  auto func = []() -> absl::Status {
    int value = 1;
    MALDOCA_ASSIGN_OR_RETURN(value, ReturnStatusOrValue(2));
    EXPECT_EQ(2, value);
    MALDOCA_ASSIGN_OR_RETURN(value, ReturnStatusOrValue(3));
    EXPECT_EQ(3, value);
    MALDOCA_ASSIGN_OR_RETURN(value, ReturnStatusOrError("EXPECTED"));
    return ReturnError("ERROR");
  };

  EXPECT_THAT(func().message(), Eq("EXPECTED"));
}

TEST(AssignOrReturn, UniquePtrWorks) {
  auto func = []() -> absl::Status {
    MALDOCA_ASSIGN_OR_RETURN(std::unique_ptr<int> ptr,
                             ReturnStatusOrPtrValue(1));
    EXPECT_EQ(*ptr, 1);
    return ReturnError("EXPECTED");
  };

  EXPECT_THAT(func().message(), Eq("EXPECTED"));
}

TEST(AssignOrReturn, UniquePtrWorksForExistingVariable) {
  auto func = []() -> absl::Status {
    std::unique_ptr<int> ptr;
    MALDOCA_ASSIGN_OR_RETURN(ptr, ReturnStatusOrPtrValue(1));
    EXPECT_EQ(*ptr, 1);

    MALDOCA_ASSIGN_OR_RETURN(ptr, ReturnStatusOrPtrValue(2));
    EXPECT_EQ(*ptr, 2);
    return ReturnError("EXPECTED");
  };

  EXPECT_THAT(func().message(), Eq("EXPECTED"));
}

TEST(ReturnIfError, Works) {
  auto func = []() -> absl::Status {
    MALDOCA_RETURN_IF_ERROR(ReturnOk());
    MALDOCA_RETURN_IF_ERROR(ReturnOk());
    MALDOCA_RETURN_IF_ERROR(ReturnError("EXPECTED"));
    return ReturnError("ERROR");
  };

  EXPECT_THAT(func().message(), Eq("EXPECTED"));
}

TEST(ReturnIfError, WorksWithLambda) {
  auto func = []() -> absl::Status {
    MALDOCA_RETURN_IF_ERROR([] { return ReturnOk(); }());
    MALDOCA_RETURN_IF_ERROR([] { return ReturnError("EXPECTED"); }());
    return ReturnError("ERROR");
  };

  EXPECT_THAT(func().message(), Eq("EXPECTED"));
}

#ifndef MALDOCA_CHROME
TEST(ReturnIfError, WorksWithBuilder) {
  auto func = []() -> absl::Status {
    MALDOCA_RETURN_IF_ERROR(ReturnOkBuilder());
    MALDOCA_RETURN_IF_ERROR(ReturnOkBuilder());
    MALDOCA_RETURN_IF_ERROR(ReturnErrorBuilder("EXPECTED"));
    return ReturnErrorBuilder("ERROR");
  };

  EXPECT_THAT(func().message(), Eq("EXPECTED"));
}

TEST(ReturnIfError, WorksWithAppend) {
  auto fail_test_if_called = []() -> std::string {
    ADD_FAILURE();
    return "FAILURE";
  };
  auto func = [&]() -> absl::Status {
    MALDOCA_RETURN_IF_ERROR(ReturnOk()) << fail_test_if_called();
    MALDOCA_RETURN_IF_ERROR(ReturnError("EXPECTED A")) << "EXPECTED B";
    return absl::OkStatus();
  };

  EXPECT_THAT(func().message(),
              AllOf(HasSubstr("EXPECTED A"), HasSubstr("EXPECTED B")));
}

TEST(AssignOrReturn, WorksWithAdaptorFunc) {
  auto fail_test_if_called = [](maldoca::StatusBuilder builder) {
    ADD_FAILURE();
    return builder;
  };
  auto adaptor = [](maldoca::StatusBuilder builder) {
    return builder << "EXPECTED B";
  };
  auto func = [&]() -> absl::Status {
    ABSL_ATTRIBUTE_UNUSED int value;
    MALDOCA_ASSIGN_OR_RETURN(value, ReturnStatusOrValue(1),
                             fail_test_if_called(_));
    MALDOCA_ASSIGN_OR_RETURN(value, ReturnStatusOrError("EXPECTED A"),
                             adaptor(_));
    return ReturnOk();
  };

  EXPECT_THAT(func().message(),
              AllOf(HasSubstr("EXPECTED A"), HasSubstr("EXPECTED B")));
}
#endif  // MALDOCA_CHROME

}  // namespace
