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

#include "third_party/private_membership/src/internal/rlwe_id_utils.h"

namespace private_membership {
namespace rlwe {

private_membership::MembershipResponse MembershipResponseMap::Get(
    RlwePlaintextId id) {
  return map_[HashRlwePlaintextId(id)];
}

void MembershipResponseMap::Update(
    RlwePlaintextId id, private_membership::MembershipResponse response) {
  map_[HashRlwePlaintextId(id)] = response;
}

bool MembershipResponseMap::Contains(RlwePlaintextId id) {
  return map_.contains(HashRlwePlaintextId(id));
}

void MembershipResponseMap::Merge(const MembershipResponseMap& other_map) {
  // Check for duplicate IDs.
  for (auto it = other_map.GetMap().begin(); it != other_map.GetMap().end();
       ++it) {
    if (!map_.contains(it->first)) {
      map_[it->first] = it->second;
    }
  }
}

}  // namespace rlwe
}  // namespace private_membership
