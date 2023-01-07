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

#include "maldoca/base/status.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::HasSubstr;

namespace maldoca {
namespace {

absl::Cord ToPayload(absl::string_view payload) { return absl::Cord(payload); }

TEST(ToPayload, Works) {
  EXPECT_EQ(ToPayload("a"), ToPayload("a"));
  EXPECT_NE(ToPayload("a"), ToPayload("b"));
}

// Check that s has the specified fields.
//
// An empty `payload_message` means the s must not contain a payload,
// otherwise the contents of payload must be equal to that returned by
// `ToPayload(payload_message)`.
//
// Note: most code should not validate Status values this way.  Use
// cs/testing/base/public/gmock_utils/status-matchers.h instead.
static void CheckStatus(const absl::Status& s, const absl::StatusCode code,
                        const std::string& message,
                        const std::string& payload_type,
                        const std::string& payload_msg) {
  SCOPED_TRACE(testing::Message() << "Where s is " << s);
  EXPECT_EQ(static_cast<int>(code), s.raw_code());
  EXPECT_EQ(code, s.code());
  EXPECT_EQ(message, s.message());

  if (code == absl::StatusCode::kOk) {
    EXPECT_TRUE(s.ok());
    EXPECT_EQ("OK", s.ToString());
  } else {
    EXPECT_TRUE(!s.ok());
    EXPECT_THAT(s.ToString(), HasSubstr(message));
  }

  if (payload_type.empty()) {
    // Doesn't make sense to expect a payload message without a type.
    ASSERT_TRUE(payload_msg.empty());
    EXPECT_FALSE(s.GetPayload(payload_type).has_value());
  } else {
    SCOPED_TRACE(testing::Message()
                 << "Expecting payload_message == "
                 << "(\"" << payload_type << "\", \"" << payload_msg << "\")");
    ASSERT_TRUE(s.GetPayload(payload_type).has_value());
    EXPECT_EQ(*s.GetPayload(payload_type), ToPayload(payload_msg));
  }
}

static void CheckStatus(const absl::Status& s,
                        const absl::StatusCode error_code,
                        const std::string& message) {
  return CheckStatus(s, error_code, message, "", "");
}

}  // namespace

TEST(Status, ConstructDefault) {
  absl::Status status;
  CheckStatus(status, absl::StatusCode::kOk, "");
}

TEST(Status, OkStatus) {
  CheckStatus(absl::OkStatus(), absl::StatusCode::kOk, "");
}

// Test that the many ways of passing an error code of zero always
// produces an OK status.
TEST(Status, ConstructWithOk) {
  EXPECT_EQ(absl::Status(), absl::OkStatus());

  EXPECT_EQ(absl::Status(absl::StatusCode::kOk, "ignored"), absl::OkStatus());
}

// Test equivalence across the various ways of constructing a Status
// with no message and no payload in the canonical space.  The current
// implementation represents these values differently from others.
TEST(Status, ConstructNoMessage) {
  const absl::Status cancelled(absl::StatusCode::kCancelled, "");
  CheckStatus(cancelled, absl::StatusCode::kCancelled, "");

  EXPECT_EQ(cancelled, absl::Status(absl::StatusCode::kCancelled, ""));
}

// Test equivalence across the various ways of constructing a Status
// with a message, no payload.
TEST(Status, ConstructWithMessage) {
  const absl::Status cancelled(absl::StatusCode::kCancelled, "message");
  CheckStatus(cancelled, absl::StatusCode::kCancelled, "message");

  EXPECT_NE(cancelled, absl::Status(absl::StatusCode::kCancelled, ""));
}

TEST(Status, EqualsSame) {
  const absl::Status a = absl::Status(absl::StatusCode::kCancelled, "message");
  const absl::Status b = absl::Status(absl::StatusCode::kCancelled, "message");
  ASSERT_EQ(a, b);
}

TEST(Status, EqualsCopy) {
  const absl::Status a = absl::Status(absl::StatusCode::kCancelled, "message");
  const absl::Status b = a;
  ASSERT_EQ(a, b);
}

TEST(Status, EqualsDifferentCode) {
  const absl::Status a = absl::Status(absl::StatusCode::kCancelled, "message");
  const absl::Status b =
      absl::Status(absl::StatusCode::kInvalidArgument, "message");
  ASSERT_NE(a, b);
}

TEST(Status, EqualsDifferentMessage) {
  const absl::Status a = absl::Status(absl::StatusCode::kCancelled, "message");
  const absl::Status b = absl::Status(absl::StatusCode::kCancelled, "another");
  ASSERT_NE(a, b);
}

TEST(Status, SetToOkNoOp) {
  absl::Status a;
  a.SetPayload("type_a", ToPayload("msg"));
  ASSERT_EQ(a, absl::OkStatus());
}

TEST(Status, NotEqualsPayload) {
  absl::Status a = absl::Status(absl::StatusCode::kCancelled, "msg");
  a.SetPayload("type_a", ToPayload(""));

  absl::Status b = absl::Status(absl::StatusCode::kCancelled, "msg");
  ASSERT_NE(a, b);
}

TEST(Status, EqualsPayloadMismatch) {
  absl::Status a = absl::Status(absl::StatusCode::kCancelled, "msg");
  a.SetPayload("type_a", ToPayload("foo"));
  absl::Status b = absl::Status(absl::StatusCode::kCancelled, "msg");
  b.SetPayload("type_a", ToPayload("bar"));

  ASSERT_NE(a, b);
}

TEST(Status, EqualsPayloadMismatchType) {
  absl::Status a = absl::Status(absl::StatusCode::kCancelled, "msg");
  a.SetPayload("type_a", ToPayload("foo"));
  absl::Status b = absl::Status(absl::StatusCode::kCancelled, "msg");
  b.SetPayload("type_b", ToPayload("bar"));

  ASSERT_NE(a, b);
}

TEST(Status, SetOverwrites) {
  absl::Status a = absl::Status(absl::StatusCode::kCancelled, "msg");
  a.SetPayload("type_a", ToPayload("foo"));
  absl::Status b = absl::Status(absl::StatusCode::kCancelled, "msg");
  b.SetPayload("type_a", ToPayload("bar"));
  ASSERT_NE(a, b);

  b.SetPayload("type_a", ToPayload("foo"));
  ASSERT_EQ(a, b);
}

TEST(Status, EraseOnEmptyIsNoOp) {
  absl::Status a = absl::Status(absl::StatusCode::kCancelled, "msg");
  a.ErasePayload("type_a");
  ASSERT_EQ(a, absl::Status(absl::StatusCode::kCancelled, "msg"));
}

TEST(Status, EraseWorks) {
  absl::Status a = absl::Status(absl::StatusCode::kCancelled, "msg");
  a.SetPayload("type_a", ToPayload("foo"));
  a.SetPayload("type_b", ToPayload("bar"));
  a.ErasePayload("type_a");

  EXPECT_FALSE(a.GetPayload("type_a").has_value());
  absl::Status expected = absl::Status(absl::StatusCode::kCancelled, "msg");
  expected.SetPayload("type_b", ToPayload("bar"));
  ASSERT_EQ(a, expected);
}

void VisitAndAssertEquals(const absl::Status& status,
                          absl::string_view find_type_url,
                          const absl::optional<absl::Cord>& expected) {
  bool already_seen = false;
  status.ForEachPayload([&already_seen, find_type_url, expected](
                            absl::string_view type_url,
                            const absl::Cord& payload) {
    if (find_type_url == type_url) {
      ASSERT_FALSE(already_seen) << find_type_url << " has already been seen";
      already_seen = true;
      ASSERT_EQ(payload, expected);
    }
  });
  ASSERT_EQ(expected.has_value(), already_seen);
}

TEST(Status, ForEachPayloadEmptyPayload) {
  absl::Status a = absl::Status(absl::StatusCode::kCancelled, "msg");
  VisitAndAssertEquals(a, "type_a", absl::nullopt);
}

TEST(Status, ForEachPayload_Multiple) {
  absl::Status a = absl::Status(absl::StatusCode::kCancelled, "msg");
  a.SetPayload("type_a", ToPayload("bar"));
  a.SetPayload("type_b", ToPayload("foo"));
  VisitAndAssertEquals(a, "type_a", absl::Cord("bar"));
  VisitAndAssertEquals(a, "type_b", absl::Cord("foo"));
}

}  // namespace maldoca
