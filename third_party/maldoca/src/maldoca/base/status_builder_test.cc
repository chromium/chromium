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

#ifndef MALDOCA_CHROME

#include "maldoca/base/status_builder.h"

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "gmock/gmock.h"
#include "google/protobuf/descriptor.h"
#include "gtest/gtest.h"
#include "maldoca/base/source_location.h"
#include "maldoca/base/status.h"
#include "maldoca/base/status_payload.h"
#include "maldoca/base/statusor.h"
#include "maldoca/base/test_payload.pb.h"
#include "maldoca/base/testing/status_matchers.h"

namespace maldoca {
namespace {

using ::maldoca::testing::StatusIs;
using ::testing::Eq;
using ::testing::HasSubstr;

// Converts a StatusBuilder to a absl::Status.
absl::Status ToStatus(const StatusBuilder& s) { return s; }

absl::Status Cancelled() {
  return absl::Status(absl::StatusCode::kCancelled, "");
}

const absl::StatusCode kZomg = absl::StatusCode::kUnimplemented;
const SourceLocation kLoc = MALDOCA_LOC;

// Converts a StatusBuilder to a StatusOr<T>.
template <typename T>
StatusOr<T> ToStatusOr(const StatusBuilder& s) {
  return s;
}

// Makes a payload proto suitable for attaching to a status.
TestPayload MakePayloadProto(const std::string& str) {
  TestPayload proto;
  proto.set_message(str);
  return proto;
}

// Makes a message set contains a payload proto, suitable for passing to the
// constructor of `Status`.
void SetTestPayload(const std::string& str, absl::Status* s) {
  TestPayload payload = MakePayloadProto(str);
  AttachPayload(s, payload);
}

absl::Status StatusWithPayload(absl::StatusCode code, const std::string& msg,
                               const std::string& str) {
  absl::Status s(code, msg);
  SetTestPayload(str, &s);
  return s;
}

absl::Status StatusWithPayload(absl::StatusCode code, const std::string& msg,
                               const std::string& str1,
                               const std::string& str2) {
  absl::Status s(code, msg);
  SetTestPayload(str1, &s);
  SetTestPayload(str2, &s);
  return s;
}

TEST(StatusBuilderTest, Ctors) {
  EXPECT_EQ(ToStatus(StatusBuilder(kZomg, MALDOCA_LOC) << "zomg"),
            absl::Status(kZomg, "zomg"));
}

TEST(StatusBuilderTest, Identity) {
  SourceLocation loc(MALDOCA_LOC);

  const std::vector<absl::Status> statuses = {
      absl::OkStatus(),
      Cancelled(),
      absl::InvalidArgumentError("yup"),
  };

  for (const absl::Status& base : statuses) {
    EXPECT_THAT(ToStatus(StatusBuilder(base, loc)), Eq(base));
    EXPECT_EQ(StatusBuilder(base, loc).ok(), base.ok());
    if (!base.ok()) {
      EXPECT_THAT(ToStatusOr<int>(StatusBuilder(base, loc)).status(), Eq(base));
    }
  }
}

TEST(StatusBuilderTest, SourceLocation) {
  const SourceLocation kLocation =
      SourceLocation::DoNotInvokeDirectly(0x42, "my_file");

  {
    const StatusBuilder builder(absl::OkStatus(), kLocation);
    EXPECT_THAT(builder.source_location().file_name(),
                Eq(kLocation.file_name()));
    EXPECT_THAT(builder.source_location().line(), Eq(kLocation.line()));
  }
}

TEST(StatusBuilderTest, ErrorCode) {
  // OK
  {
    const StatusBuilder builder(absl::OkStatus(), kLoc);
    EXPECT_TRUE(builder.ok());
    EXPECT_THAT(builder.code(), Eq(absl::StatusCode::kOk));
    EXPECT_FALSE(builder.Is(kZomg));
  }

  // Non-OK canonical code
  {
    const StatusBuilder builder(absl::StatusCode::kInvalidArgument, kLoc);
    EXPECT_FALSE(builder.ok());
    EXPECT_THAT(builder.code(), Eq(absl::StatusCode::kInvalidArgument));
    EXPECT_FALSE(builder.Is(kZomg));
  }
}

TEST(StatusBuilderTest, OkIgnoresStuff) {
  EXPECT_THAT(ToStatus(StatusBuilder(absl::OkStatus(), kLoc) << "booyah"),
              Eq(absl::OkStatus()));
  EXPECT_THAT(
      ToStatus(
          StatusBuilder(absl::OkStatus(), kLoc).Attach(MakePayloadProto("omg"))
          << "zombies"),
      Eq(absl::OkStatus()));
}

TEST(StatusBuilderTest, Streaming) {
  EXPECT_THAT(ToStatus(StatusBuilder(Cancelled(), kLoc) << "booyah"),
              Eq(absl::CancelledError("booyah")));
  EXPECT_THAT(
      ToStatus(StatusBuilder(absl::AbortedError("hello"), kLoc) << "world"),
      Eq(absl::AbortedError("hello; world")));
}

TEST(StatusBuilderTest, Prepend) {
  EXPECT_THAT(
      ToStatus(StatusBuilder(Cancelled(), kLoc).SetPrepend() << "booyah"),
      Eq(absl::CancelledError("booyah")));
  EXPECT_THAT(
      ToStatus(StatusBuilder(absl::AbortedError(" hello"), kLoc).SetPrepend()
               << "world"),
      Eq(absl::AbortedError("world hello")));
}

TEST(StatusBuilderTest, Append) {
  EXPECT_THAT(
      ToStatus(StatusBuilder(Cancelled(), kLoc).SetAppend() << "booyah"),
      Eq(absl::CancelledError("booyah")));
  EXPECT_THAT(
      ToStatus(StatusBuilder(absl::AbortedError("hello"), kLoc).SetAppend()
               << " world"),
      Eq(absl::AbortedError("hello world")));
}

TEST(StatusBuilderTest, SetErrorCode) {
  EXPECT_THAT(
      ToStatus(StatusBuilder(Cancelled(), kLoc)
                   .Attach(MakePayloadProto("oops"))
                   .SetErrorCode(absl::StatusCode::kFailedPrecondition)),
      Eq(StatusWithPayload(absl::StatusCode::kFailedPrecondition, "", "oops")));
  EXPECT_THAT(
      ToStatus(StatusBuilder(absl::CancelledError("monkey"), kLoc)
                   .SetErrorCode(absl::StatusCode::kFailedPrecondition)
               << "taco"),
      Eq(absl::Status(absl::StatusCode::kFailedPrecondition, "monkey; taco")));
}

TEST(StatusBuilderTest, Attach) {
  EXPECT_THAT(
      ToStatus(
          StatusBuilder(Cancelled(), kLoc).Attach(MakePayloadProto("oops"))),
      Eq(StatusWithPayload(absl::StatusCode::kCancelled, "", "oops")));
  EXPECT_THAT(
      ToStatus(StatusBuilder(Cancelled(), kLoc).Attach(MakePayloadProto("boom"))
               << "stick"),
      Eq(StatusWithPayload(absl::StatusCode::kCancelled, "stick", "boom")));
}

TEST(StatusBuilderTest, MultiAttach) {
  EXPECT_THAT(
      ToStatus(StatusBuilder(Cancelled(), kLoc)
                   .Attach(MakePayloadProto("msg1"))
                   .Attach(MakePayloadProto("msg2"))),
      Eq(StatusWithPayload(absl::StatusCode::kCancelled, "", "msg1", "msg2")));
  EXPECT_THAT(
      ToStatus(StatusBuilder(Cancelled(), kLoc).Attach(MakePayloadProto("boom"))
               << "stick"),
      Eq(StatusWithPayload(absl::StatusCode::kCancelled, "stick", "boom")));
}

// This structure holds the details for testing a single canonical error code,
// its creator, and its classifier.
struct CanonicalErrorTest {
  absl::StatusCode code;
  StatusBuilder (*creator)(SourceLocation);
};

constexpr CanonicalErrorTest kCanonicalErrorTests[]{
    {absl::StatusCode::kAborted, AbortedErrorBuilder},
    {absl::StatusCode::kAlreadyExists, AlreadyExistsErrorBuilder},
    {absl::StatusCode::kCancelled, CancelledErrorBuilder},
    {absl::StatusCode::kDataLoss, DataLossErrorBuilder},
    {absl::StatusCode::kDeadlineExceeded, DeadlineExceededErrorBuilder},
    {absl::StatusCode::kFailedPrecondition, FailedPreconditionErrorBuilder},
    {absl::StatusCode::kInternal, InternalErrorBuilder},
    {absl::StatusCode::kInvalidArgument, InvalidArgumentErrorBuilder},
    {absl::StatusCode::kNotFound, NotFoundErrorBuilder},
    {absl::StatusCode::kOutOfRange, OutOfRangeErrorBuilder},
    {absl::StatusCode::kPermissionDenied, PermissionDeniedErrorBuilder},
    {absl::StatusCode::kUnauthenticated, UnauthenticatedErrorBuilder},
    {absl::StatusCode::kResourceExhausted, ResourceExhaustedErrorBuilder},
    {absl::StatusCode::kUnavailable, UnavailableErrorBuilder},
    {absl::StatusCode::kUnimplemented, UnimplementedErrorBuilder},
    {absl::StatusCode::kUnknown, UnknownErrorBuilder},
};

TEST(CanonicalErrorsTest, CreateAndClassify) {
  for (const auto& test : kCanonicalErrorTests) {
    SCOPED_TRACE(absl::StrCat("", absl::StatusCodeToString(test.code)));

    // Ensure that the creator does, in fact, create status objects in the
    // canonical space, with the expected error code and message.
    std::string message =
        absl::StrCat("error code ", test.code, " test message");
    absl::Status status = test.creator(MALDOCA_LOC) << message;
    EXPECT_EQ(test.code, status.code());
    EXPECT_EQ(message, status.message());
  }
}

}  // namespace
}  // namespace maldoca

#else
#include "gtest/gtest.h"

#endif  // MALDOCA_CHROME
