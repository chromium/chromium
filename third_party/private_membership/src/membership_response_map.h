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

#ifndef THIRD_PARTY_PRIVATE_MEMBERSHIP_SRC_MEMBERSHIP_RESPONSE_MAP_H_
#define THIRD_PARTY_PRIVATE_MEMBERSHIP_SRC_MEMBERSHIP_RESPONSE_MAP_H_

#include "third_party/private_membership/src/private_membership.pb.h"
#include "third_party/private_membership/base/private_membership_export.h"
#include "third_party/private_membership/src/private_membership_rlwe.pb.h"
#include "absl/container/flat_hash_map.h"

namespace private_membership {
namespace rlwe {

class PRIVATE_MEMBERSHIP_EXPORT MembershipResponseMap {
 public:
  MembershipResponseMap() = default;

  // Get the MembershipResponse associated with id.
  private_membership::MembershipResponse Get(RlwePlaintextId id);

  // Update the Membership Response associated with id.
  void Update(RlwePlaintextId id,
              private_membership::MembershipResponse response);

  // Merges current map with argument map. Duplicate IDs that appear in
  // other_map will not be reflected in the newly merged map.
  void Merge(const MembershipResponseMap& other_map);

  const absl::flat_hash_map<std::string,
                            private_membership::MembershipResponse>&
  GetMap() const {
    return map_;
  }

  // Returns whether map contains passed id.
  bool Contains(RlwePlaintextId id);

 private:
  // Map storing hashes of RlwePlaintextId to responses.
  absl::flat_hash_map<std::string, private_membership::MembershipResponse> map_;
};

}  // namespace rlwe
}  // namespace private_membership

#endif  // THIRD_PARTY_PRIVATE_MEMBERSHIP_SRC_MEMBERSHIP_RESPONSE_MAP_H_
