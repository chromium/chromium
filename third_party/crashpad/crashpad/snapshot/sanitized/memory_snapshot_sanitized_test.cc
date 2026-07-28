// Copyright 2026 The Crashpad Authors
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

#include "snapshot/sanitized/memory_snapshot_sanitized.h"

#include <stdint.h>
#include <string.h>

#include <vector>

#include "base/containers/heap_array.h"
#include "gtest/gtest.h"
#include "util/misc/range_set.h"

namespace crashpad {
namespace test {
namespace {

constexpr uint8_t kFillByte = 0x55;

class BufferMemorySnapshot final : public MemorySnapshot {
 public:
  BufferMemorySnapshot(uint64_t address, size_t size)
      : address_(address), size_(size) {}

  BufferMemorySnapshot(const BufferMemorySnapshot&) = delete;
  BufferMemorySnapshot& operator=(const BufferMemorySnapshot&) = delete;

  uint64_t Address() const override { return address_; }
  size_t Size() const override { return size_; }

  bool Read(Delegate* delegate) const override {
    if (size_ == 0) {
      return delegate->MemorySnapshotDelegateRead(nullptr, 0);
    }
    auto buffer = base::HeapArray<uint8_t>::Uninit(size_);
    memset(buffer.data(), kFillByte, buffer.size());
    return delegate->MemorySnapshotDelegateRead(buffer.data(), buffer.size());
  }

  const MemorySnapshot* MergeWithOtherSnapshot(
      const MemorySnapshot*) const override {
    return nullptr;
  }

 private:
  uint64_t address_;
  size_t size_;
};

class CapturingDelegate : public MemorySnapshot::Delegate {
 public:
  bool MemorySnapshotDelegateRead(void* data, size_t size) override {
    captured_.assign(static_cast<uint8_t*>(data),
                     static_cast<uint8_t*>(data) + size);
    return true;
  }

  const std::vector<uint8_t>& captured() const { return captured_; }

 private:
  std::vector<uint8_t> captured_;
};

void ExpectSanitizedShortUnalignedRegion(uint64_t address,
                                         size_t size,
                                         bool is_64_bit) {
  BufferMemorySnapshot wrapped(address, size);
  RangeSet ranges;
  internal::MemorySnapshotSanitized sanitized(&wrapped, &ranges, is_64_bit);

  CapturingDelegate delegate;
  ASSERT_TRUE(sanitized.Read(&delegate));
  ASSERT_EQ(delegate.captured().size(), size);

  // The region is shorter than a word and contains no pointer-aligned word, so
  // every byte should have been defaced.
  for (size_t index = 0; index < size; ++index) {
    EXPECT_NE(delegate.captured()[index], kFillByte)
        << "address=" << address << " size=" << size << " index=" << index;
  }
}

TEST(MemorySnapshotSanitized, ShortUnalignedRegion64) {
  for (uint64_t offset = 1; offset < sizeof(uint64_t); ++offset) {
    for (size_t size = 1; size < sizeof(uint64_t); ++size) {
      ExpectSanitizedShortUnalignedRegion(
          0x1000 + offset, size, /*is_64_bit=*/true);
    }
  }
}

TEST(MemorySnapshotSanitized, ShortUnalignedRegion32) {
  for (uint64_t offset = 1; offset < sizeof(uint32_t); ++offset) {
    for (size_t size = 1; size < sizeof(uint32_t); ++size) {
      ExpectSanitizedShortUnalignedRegion(
          0x1000 + offset, size, /*is_64_bit=*/false);
    }
  }
}

}  // namespace
}  // namespace test
}  // namespace crashpad
