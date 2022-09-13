// Copyright 2022 The Crashpad Authors
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

#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace crashpad {
namespace test {
namespace {

using internal::MemorySnapshotIOSIntermediateDump;

const vm_address_t kDefaultAddress = 0x1000;

class ReadToString : public crashpad::MemorySnapshot::Delegate {
 public:
  const std::string& result() { return result_; }

 private:
  // MemorySnapshot::Delegate:
  bool MemorySnapshotDelegateRead(void* data, size_t size) override {
    result_ = std::string(reinterpret_cast<const char*>(data), size);
    return true;
  }

  std::string result_;
};

std::unique_ptr<MemorySnapshotIOSIntermediateDump> CreateMemorySnapshot(
    vm_address_t address,
    std::vector<uint8_t>& data) {
  auto memory = std::make_unique<MemorySnapshotIOSIntermediateDump>();
  memory->Initialize(
      address, reinterpret_cast<const vm_address_t>(data.data()), data.size());
  return memory;
}

TEST(MemorySnapshotIOSIntermediateDumpTest, MergeSame) {
  std::vector<uint8_t> data(10, 'a');
  auto memory = CreateMemorySnapshot(kDefaultAddress, data);
  std::unique_ptr<const MemorySnapshot> merged(
      memory->MergeWithOtherSnapshot(memory.get()));
  EXPECT_EQ(merged->Address(), kDefaultAddress);
  EXPECT_EQ(merged->Size(), data.size());
  ReadToString delegate;
  merged->Read(&delegate);
  EXPECT_EQ(delegate.result(), "aaaaaaaaaa");
}

TEST(MemorySnapshotIOSIntermediateDumpTest, MergeNoOverlap) {
  std::vector<uint8_t> data1(10, 'a');
  auto memory1 = CreateMemorySnapshot(kDefaultAddress, data1);

  std::vector<uint8_t> data2(10, 'b');
  auto memory2 = CreateMemorySnapshot(kDefaultAddress + 10, data2);

  std::unique_ptr<const MemorySnapshot> merged(
      memory1->MergeWithOtherSnapshot(memory2.get()));
  EXPECT_EQ(merged->Address(), kDefaultAddress);
  EXPECT_EQ(merged->Size(), 20u);
  ReadToString delegate;
  merged->Read(&delegate);
  EXPECT_EQ(delegate.result(), "aaaaaaaaaabbbbbbbbbb");
}

TEST(MemorySnapshotIOSIntermediateDumpTest, MergePartial) {
  std::vector<uint8_t> data1(10, 'a');
  auto memory1 = CreateMemorySnapshot(kDefaultAddress, data1);

  std::vector<uint8_t> data2(10, 'b');
  auto memory2 = CreateMemorySnapshot(kDefaultAddress + 5, data2);

  std::unique_ptr<const MemorySnapshot> merged(
      memory1->MergeWithOtherSnapshot(memory2.get()));
  EXPECT_EQ(merged->Address(), kDefaultAddress);
  EXPECT_EQ(merged->Size(), 15u);
  ReadToString delegate;
  merged->Read(&delegate);
  EXPECT_EQ(delegate.result(), "aaaaabbbbbbbbbb");
}

TEST(MemorySnapshotIOSIntermediateDumpTest, NoMerge) {
  std::vector<uint8_t> data1(10, 'a');
  auto memory1 = CreateMemorySnapshot(kDefaultAddress, data1);

  std::vector<uint8_t> data2(10, 'b');
  auto memory2 = CreateMemorySnapshot(kDefaultAddress + 20, data2);

  std::unique_ptr<const MemorySnapshot> merged(
      memory1->MergeWithOtherSnapshot(memory2.get()));
  EXPECT_EQ(merged.get(), nullptr);
}

TEST(MemorySnapshotIOSIntermediateDumpTest, EnvelopeBiggerFirst) {
  std::vector<uint8_t> data1(30, 'a');
  auto memory1 = CreateMemorySnapshot(kDefaultAddress, data1);

  std::vector<uint8_t> data2(10, 'b');
  auto memory2 = CreateMemorySnapshot(kDefaultAddress + 15, data2);

  std::unique_ptr<const MemorySnapshot> merged(
      memory1->MergeWithOtherSnapshot(memory2.get()));
  EXPECT_EQ(merged->Address(), kDefaultAddress);
  EXPECT_EQ(merged->Size(), data1.size());

  ReadToString delegate;
  merged->Read(&delegate);
  EXPECT_EQ(delegate.result(), "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
}

TEST(MemorySnapshotIOSIntermediateDumpTest, EnvelopeBiggerSecond) {
  std::vector<uint8_t> data1(10, 'a');
  auto memory1 = CreateMemorySnapshot(kDefaultAddress, data1);

  std::vector<uint8_t> data2(20, 'b');
  auto memory2 = CreateMemorySnapshot(kDefaultAddress, data2);

  std::unique_ptr<const MemorySnapshot> merged(
      memory1->MergeWithOtherSnapshot(memory2.get()));
  EXPECT_EQ(merged->Address(), kDefaultAddress);
  EXPECT_EQ(merged->Size(), data2.size());

  ReadToString delegate;
  merged->Read(&delegate);
  EXPECT_EQ(delegate.result(), "bbbbbbbbbbbbbbbbbbbb");
}

TEST(MemorySnapshotIOSIntermediateDumpTest, SmallerAddressSecond) {
  std::vector<uint8_t> data1(10, 'a');
  auto memory1 = CreateMemorySnapshot(kDefaultAddress, data1);

  std::vector<uint8_t> data2(20, 'b');
  auto memory2 = CreateMemorySnapshot(kDefaultAddress - 10, data2);

  std::unique_ptr<const MemorySnapshot> merged(
      memory1->MergeWithOtherSnapshot(memory2.get()));
  EXPECT_EQ(merged->Address(), kDefaultAddress - 10);
  EXPECT_EQ(merged->Size(), data2.size());
  ReadToString delegate;
  merged->Read(&delegate);
  EXPECT_EQ(delegate.result(), "bbbbbbbbbbbbbbbbbbbb");
}

}  // namespace
}  // namespace test
}  // namespace crashpad
