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

#include "third_party/private_membership/src/membership_response_map.h"

#include "third_party/private_membership/src/private_membership.pb.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "third_party/shell-encryption/src/testing/protobuf_matchers.h"

namespace private_membership {
namespace rlwe {
namespace {

using ::rlwe::testing::EqualsProto;

RlwePlaintextId CreateRlwePlaintextId(absl::string_view non_sensitive_id,
                                      absl::string_view sensitive_id) {
  RlwePlaintextId id;
  id.set_non_sensitive_id(std::string(non_sensitive_id));
  id.set_sensitive_id(std::string(sensitive_id));
  return id;
}

TEST(MembershipResponseMapTest, GetEmpty) {
  MembershipResponseMap map;
  EXPECT_THAT(map.Get(CreateRlwePlaintextId("1", "2")),
              EqualsProto(private_membership::MembershipResponse()));
}

TEST(MembershipResponseMapTest, UpdateAndGet) {
  MembershipResponseMap map;
  RlwePlaintextId id = CreateRlwePlaintextId("a", "b");
  EXPECT_THAT(map.Get(id),
              EqualsProto(private_membership::MembershipResponse()));

  private_membership::MembershipResponse resp;
  resp.set_is_member(true);
  map.Update(id, resp);
  EXPECT_THAT(map.Get(id), EqualsProto(resp));
}

TEST(MembershipResponseMapTest, MultipleUpdateAndGet) {
  const int num_rounds = 1000;
  std::vector<RlwePlaintextId> ids(num_rounds);
  std::vector<private_membership::MembershipResponse> resps(num_rounds);

  MembershipResponseMap map;
  for (int i = 0; i < num_rounds; ++i) {
    ids[i] =
        CreateRlwePlaintextId(absl::StrCat("nsid", i), absl::StrCat("sid", i));
    resps[i].set_value(absl::StrCat("value", i));
    map.Update(ids[i], resps[i]);
  }
  for (int i = 0; i < num_rounds; ++i) {
    EXPECT_THAT(map.Get(ids[i]), EqualsProto(resps[i]));
  }
}

}  // namespace
}  // namespace rlwe
}  // namespace private_membership
