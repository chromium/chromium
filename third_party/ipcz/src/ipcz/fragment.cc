// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/fragment.h"

#include <cstdint>

#include "third_party/abseil-cpp/absl/base/macros.h"

namespace ipcz {

Fragment::Fragment(const FragmentDescriptor& descriptor, void* address)
    : descriptor_(descriptor), address_(address) {
  // If `address` is non-null, the descriptor must also be. Note that the
  // inverse is not true, as a pending fragment may have a null mapped address
  // but a non-null descriptor.
  ABSL_ASSERT(address == nullptr || !descriptor_.is_null());
}

Fragment::Fragment(const Fragment&) = default;

Fragment& Fragment::operator=(const Fragment&) = default;

}  // namespace ipcz
