// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/node_name.h"

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <string>
#include <type_traits>

#include "third_party/abseil-cpp/absl/base/macros.h"

namespace ipcz {

static_assert(std::is_standard_layout<NodeName>::value, "Invalid NodeName");

std::string NodeName::ToString() const {
  std::string name(33, 0);
  int length = snprintf(name.data(), name.size(), "%016" PRIx64 "%016" PRIx64,
                        high_, low_);
  ABSL_ASSERT(length == 32);
  return name;
}

}  // namespace ipcz
