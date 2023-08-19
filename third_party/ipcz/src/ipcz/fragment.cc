// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/fragment.h"

#include <cstdint>

#include "ipcz/driver_memory_mapping.h"
#include "ipcz/fragment_descriptor.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "util/safe_math.h"

namespace ipcz {

// static
Fragment Fragment::MappedFromDescriptor(const FragmentDescriptor& descriptor,
                                        DriverMemoryMapping& mapping) {
  if (descriptor.is_null()) {
    return {};
  }

  const uint32_t end = SaturatedAdd(descriptor.offset(), descriptor.size());
  if (end > mapping.bytes().size()) {
    return {};
  }
  return Fragment{descriptor, mapping.address_at(descriptor.offset())};
}

// static
Fragment Fragment::PendingFromDescriptor(const FragmentDescriptor& descriptor) {
  return Fragment{descriptor, nullptr};
}

// static
Fragment Fragment::FromDescriptorUnsafe(const FragmentDescriptor& descriptor,
                                        void* base_address) {
  return Fragment{descriptor, base_address};
}

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
