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

#include "maldoca/base/status_payload.h"

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "gmock/gmock.h"
#include "google/protobuf/any.pb.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"
#include "maldoca/base/status.h"
#include "maldoca/base/test_payload.pb.h"

namespace maldoca {

std::vector<std::pair<std::string, absl::Cord>> GetEntries(
    const absl::Status& status) {
  std::vector<std::pair<std::string, absl::Cord>> ret;
  status.ForEachPayload(
      [&ret](absl::string_view type_url, const absl::Cord& payload) {
        ret.emplace_back(std::string(type_url), payload);
      });
  return ret;
}

TEST(StatusPayload, EncodeSameAsAny) {
  // The intent is that we are encoding the same way an any proto would encode
  // the payload, without actually invoking any directly. Double check that
  // this is the case.
  TestPayload payload;
  payload.set_message("test value");
  google::protobuf::Any any;
  any.PackFrom(payload);

  absl::Status status(absl::StatusCode::kCancelled, "");
  AttachPayload(&status, payload);
  std::vector<std::pair<std::string, absl::Cord>> entries = GetEntries(status);
  ASSERT_EQ(entries.size(), 1);
  EXPECT_EQ(entries[0].first, any.type_url());
  EXPECT_EQ(entries[0].second, any.value());
}

TEST(StatusPayload, AttachPayload) {
  absl::Status status = absl::Status(absl::StatusCode::kCancelled, "");
  TestPayload payload;
  payload.set_message("message");

  AttachPayload(&status, payload);
  std::vector<std::pair<std::string, absl::Cord>> entries = GetEntries(status);
  ASSERT_EQ(entries.size(), 1);

  EXPECT_EQ(entries[0].first, "type.googleapis.com/maldoca.TestPayload");
  TestPayload actual_payload;
  EXPECT_TRUE(actual_payload.ParseFromString(std::string(entries[0].second)));
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(actual_payload,
                                                                 payload));
}

TEST(StatusPayload, AttachPayload_OverwriteSameType) {
  absl::Status status = absl::Status(absl::StatusCode::kCancelled, "");
  TestPayload payload1;
  payload1.set_message("message1");
  TestPayload payload2;
  payload2.set_message("message2");

  AttachPayload(&status, payload1);
  AttachPayload(&status, payload2);

  std::vector<std::pair<std::string, absl::Cord>> entries = GetEntries(status);
  ASSERT_EQ(entries.size(), 1);

  TestPayload actual_payload;
  EXPECT_TRUE(actual_payload.ParseFromString(std::string(entries[0].second)));

  EXPECT_EQ(entries[0].first, "type.googleapis.com/maldoca.TestPayload");
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(actual_payload,
                                                                 payload2));
}

}  // namespace maldoca
