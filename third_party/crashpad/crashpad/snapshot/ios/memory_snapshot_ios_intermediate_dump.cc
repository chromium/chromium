// Copyright 2020 The Crashpad Authors
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

#include "base/check_op.h"

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
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  auto other_snapshot =
      reinterpret_cast<const MemorySnapshotIOSIntermediateDump*>(other);

  INITIALIZATION_STATE_DCHECK_VALID(other_snapshot->initialized_);
  if (other_snapshot->address_ < address_) {
    return other_snapshot->MergeWithOtherSnapshot(this);
  }

  CheckedRange<uint64_t, size_t> merged(0, 0);
  if (!LoggingDetermineMergedRange(this, other, &merged))
    return nullptr;

  auto result = std::make_unique<MemorySnapshotIOSIntermediateDump>();
  result->Initialize(merged.base(), data_, merged.size());
  if (size_ == merged.size()) {
    return result.release();
  }

  const uint8_t* data = reinterpret_cast<const uint8_t*>(data_);
  const uint8_t* other_data =
      reinterpret_cast<const uint8_t*>(other_snapshot->data_);
  vm_size_t overlap = merged.size() - other_snapshot->size_;
  result->merged_data_.reserve(merged.size());
  result->merged_data_.insert(result->merged_data_.end(), data, data + overlap);
  result->merged_data_.insert(result->merged_data_.end(),
                              other_data,
                              other_data + other_snapshot->size_);
  result->data_ =
      reinterpret_cast<const vm_address_t>(result->merged_data_.data());
  DCHECK_EQ(result->merged_data_.size(), merged.size());
  return result.release();
}

}  // namespace internal
}  // namespace crashpad
