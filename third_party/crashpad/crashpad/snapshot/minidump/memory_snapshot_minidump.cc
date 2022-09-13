// Copyright 2018 The Crashpad Authors
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

#include "snapshot/minidump/memory_snapshot_minidump.h"

#include <memory>

#include "base/numerics/safe_math.h"

namespace crashpad {
namespace internal {

MemorySnapshotMinidump::MemorySnapshotMinidump()
    : MemorySnapshot(),
      address_(0),
      data_(),
      initialized_() {}

MemorySnapshotMinidump::~MemorySnapshotMinidump() {}

bool MemorySnapshotMinidump::Initialize(FileReaderInterface* file_reader,
                                        RVA location) {
  INITIALIZATION_STATE_SET_INITIALIZING(initialized_);

  MINIDUMP_MEMORY_DESCRIPTOR descriptor;

  if (!file_reader->SeekSet(location)) {
    return false;
  }

  if (!file_reader->ReadExactly(&descriptor, sizeof(descriptor))) {
    return false;
  }

  address_ = descriptor.StartOfMemoryRange;
  data_.resize(descriptor.Memory.DataSize);

  if (!file_reader->SeekSet(descriptor.Memory.Rva)) {
    return false;
  }

  if (!file_reader->ReadExactly(data_.data(), data_.size())) {
    return false;
  }

  INITIALIZATION_STATE_SET_VALID(initialized_);
  return true;
}

uint64_t MemorySnapshotMinidump::Address() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return address_;
}

size_t MemorySnapshotMinidump::Size() const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return data_.size();
}

bool MemorySnapshotMinidump::Read(Delegate* delegate) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);
  return delegate->MemorySnapshotDelegateRead(
      const_cast<uint8_t*>(data_.data()), data_.size());
}

const MemorySnapshot* MemorySnapshotMinidump::MergeWithOtherSnapshot(
    const MemorySnapshot* other) const {
  INITIALIZATION_STATE_DCHECK_VALID(initialized_);

  // TODO: Verify type of other
  auto other_cast = reinterpret_cast<const MemorySnapshotMinidump*>(other);

  INITIALIZATION_STATE_DCHECK_VALID(other_cast->initialized_);

  if (other_cast->address_ < address_) {
    return other_cast->MergeWithOtherSnapshot(this);
  }

  CheckedRange<uint64_t, size_t> merged(0, 0);
  if (!LoggingDetermineMergedRange(this, other, &merged)) {
    return nullptr;
  }

  auto result = std::make_unique<MemorySnapshotMinidump>();
  result->address_ = merged.base();
  result->data_ = data_;

  if (result->data_.size() == merged.size()) {
    return result.release();
  }

  result->data_.resize(
      base::checked_cast<size_t>(other_cast->address_ - address_));
  result->data_.insert(result->data_.end(), other_cast->data_.begin(),
                       other_cast->data_.end());
  return result.release();
}

} // namespace internal
} // namespace crashpad
