// Copyright 2020 The Chromium Authors. All rights reserved.
// Copyright 2014 Blake Embrey (hello@blakeembrey.com)
// Use of this source code is governed by an MIT-style license that can be
// found in the LICENSE file or at https://opensource.org/licenses/MIT.

#include "third_party/liburlpattern/pattern.h"

#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace liburlpattern {

std::ostream& operator<<(std::ostream& o, Part part) {
  o << "{ type:" << static_cast<int>(part.type) << ", name:" << part.name
    << ", prefix:" << part.prefix << ", value:" << part.value
    << ", suffix:" << part.suffix
    << ", modifier:" << static_cast<int>(part.modifier) << " }";
  return o;
}

Part::Part(PartType t, std::string v, Modifier m)
    : type(t), value(std::move(v)), modifier(m) {
  ABSL_ASSERT(type == PartType::kFixed);
}

Part::Part(PartType t,
           std::string n,
           std::string p,
           std::string v,
           std::string s,
           Modifier m)
    : type(t),
      name(std::move(n)),
      prefix(std::move(p)),
      value(std::move(v)),
      suffix(std::move(s)),
      modifier(m) {
  ABSL_ASSERT(type != PartType::kFixed);
  ABSL_ASSERT(!name.empty());
  if (type == PartType::kFullWildcard || type == PartType::kSegmentWildcard)
    ABSL_ASSERT(value.empty());
}

Pattern::Pattern(std::vector<Part> part_list)
    : part_list_(std::move(part_list)) {}

}  // namespace liburlpattern
