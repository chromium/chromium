// Copyright 2020 The Crashpad Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "snapshot/ios/memory_snapshot_ios_intermediate_dump.h"

namespace crashpad {
namespace internal {

void MemorySnapshotIOSIntermediateDump::Initialize(vm_address_t address,
                                                   vm_address_t data,
                                                   vm_size_t size) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);
  address_ = address;
  data_ = data;
  size_ = base::checked_cast<size_t>(size);
  INITIALIZATION_STATE_SET_VALID(initialized_);
}

uint64_t MemorySnapshotIOSIntermediateDump::Address() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return address_;
}

size_t MemorySnapshotIOSIntermediateDump::Size() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return size_;
}

bool MemorySnapshotIOSIntermediateDump::Read(Delegate* delegate) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  if (size_ == 0) {
    return delegate->MemorySnapshotDelegateRead(nullptr, size_);
  }

  return delegate->MemorySnapshotDelegateRead(reinterpret_cast<void*>(data_),
                                              size_);
}

const MemorySnapshot* MemorySnapshotIOSIntermediateDump::MergeWithOtherSnapshot(
    const MemorySnapshot* other) const {
  CheckedRange<uint64_t, size_t> merged(0, 0);
  if (!LoggingDetermineMergedRange(this, other, &merged))
    return nullptr;

  auto result = std::make_unique<MemorySnapshotIOSIntermediateDump>();
  result->Initialize(merged.base(), data_, merged.size());
  return result.release();
}

}  // namespace internal
}  // namespace crashpad
