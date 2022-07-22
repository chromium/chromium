// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/node_name.h"

#include <string>
#include <type_traits>

#include "third_party/abseil-cpp/absl/base/macros.h"
#include "third_party/abseil-cpp/absl/strings/str_cat.h"

namespace ipcz {

static_assert(std::is_standard_layout<NodeName>::value, "Invalid NodeName");

std::string NodeName::ToString() const {
  return absl::StrCat(absl::Hex(high_, absl::kZeroPad16),
                      absl::Hex(low_, absl::kZeroPad16));
}

}  // namespace ipcz
