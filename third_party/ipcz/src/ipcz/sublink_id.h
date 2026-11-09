// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPCZ_SRC_IPCZ_SUBLINK_ID_H_
#define IPCZ_SRC_IPCZ_SUBLINK_ID_H_

#include <cstdint>
#include <ostream>

#include "base/types/strong_alias.h"
#include "ipcz/link_side.h"

namespace ipcz {

// Identifies a specific subsidiary link along a NodeLink. Each sublink is a
// path between a unique pair of Router instances, one on each linked node. New
// SublinkIds are allocated atomically by either side of the NodeLink.
using SublinkId = base::StrongAlias<class SublinkIdTag, uint64_t>;

inline std::ostream& operator<<(std::ostream& stream, const SublinkId& id) {
  // For better log readability, output only the numeric value of the lower bits
  // with an A or B suffix to represent the high bit.
  constexpr uint64_t kIdMask = (1ull << kLinkSideBIdBit) - 1;
  stream << (id.value() & kIdMask)
         << (id.value() >> kLinkSideBIdBit ? ".B" : ".A");
  return stream;
}

}  // namespace ipcz

#endif  // IPCZ_SRC_IPCZ_SUBLINK_ID_H_
