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
  id.set_non_sensitive_id(std::string(
      non_sensitive_id));
  id.set_sensitive_id(
      std::string(sensitive_id));
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

TEST(MembershipResponseMapTest, Contains) {
  MembershipResponseMap map;
  RlwePlaintextId id = CreateRlwePlaintextId("a", "b");
  EXPECT_FALSE(map.Contains(id));

  private_membership::MembershipResponse resp;
  resp.set_is_member(true);
  map.Update(id, resp);
  EXPECT_TRUE(map.Contains(id));
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

TEST(MembershipResponseMapTest, Merge) {
  const int num_items = 50;
  std::vector<RlwePlaintextId> ids(num_items);
  std::vector<private_membership::MembershipResponse> resps(num_items);

  MembershipResponseMap map1;
  MembershipResponseMap map2;
  for (int i = 0; i < num_items; ++i) {
    ids[i] =
        CreateRlwePlaintextId(absl::StrCat("nsid", i), absl::StrCat("sid", i));
    resps[i].set_value(absl::StrCat("value", i));
    if (i % 2 == 0) {
      map1.Update(ids[i], resps[i]);
    } else {
      map2.Update(ids[i], resps[i]);
    }
  }
  map1.Merge(map2);
  for (int i = 0; i < num_items; ++i) {
    EXPECT_THAT(map1.Get(ids[i]), EqualsProto(resps[i]));
  }
}

TEST(MembershipResponseMapTest, MergeWithDuplicateIds) {
  RlwePlaintextId id = CreateRlwePlaintextId("nsid", "sid");
  private_membership::MembershipResponse resp1;
  resp1.set_value("value1");
  private_membership::MembershipResponse resp2;
  resp2.set_value("value2");

  MembershipResponseMap map1;
  map1.Update(id, resp1);
  MembershipResponseMap map2;
  map2.Update(id, resp2);
  map1.Merge(map2);

  EXPECT_THAT(map1.Get(id), EqualsProto(resp1));
}

}  // namespace
}  // namespace rlwe
}  // namespace private_membership
