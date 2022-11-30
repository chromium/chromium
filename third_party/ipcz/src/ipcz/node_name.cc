// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/node_name.h"

#include <atomic>
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

void NodeName::StoreRelease(const NodeName& name) {
  reinterpret_cast<std::atomic<uint64_t>*>(&high_)->store(
      name.high_, std::memory_order_relaxed);
  reinterpret_cast<std::atomic<uint64_t>*>(&low_)->store(
      name.low_, std::memory_order_release);
}

NodeName NodeName::LoadAcquire() {
  NodeName name;
  name.high_ = reinterpret_cast<std::atomic<uint64_t>*>(&high_)->load(
      std::memory_order_acquire);
  name.low_ = reinterpret_cast<std::atomic<uint64_t>*>(&low_)->load(
      std::memory_order_relaxed);
  return name;
}

}  // namespace ipcz
