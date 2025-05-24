// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/liburlpattern/part.h"

#include <cctype>
#include <ostream>
#include <string>

#include "third_party/abseil-cpp/absl/base/macros.h"

namespace liburlpattern {

std::ostream& operator<<(std::ostream& o, const Part& part) {
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
  if (type == PartType::kFullWildcard || type == PartType::kSegmentWildcard) {
    ABSL_ASSERT(value.empty());
  }
}

bool Part::HasCustomName() const {
  // Determine if the part name was custom, like `:foo`, or an
  // automatically assigned numeric value.  Since custom group
  // names follow javascript identifier rules the first character
  // cannot be a digit, so that is all we need to check here.
  return !name.empty() && !std::isdigit(name[0]);
}

}  // namespace liburlpattern
