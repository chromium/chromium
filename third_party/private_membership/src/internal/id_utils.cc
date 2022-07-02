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

#include <string>

namespace private_membership {

std::string PadOrTruncate(absl::string_view in, int len) {
  if (len <= in.size()) {
    return std::string(in.substr(0, len));
  }
  std::string ret(in);
  ret.resize(len, '0');
  return ret;
}

}  // namespace private_membership
