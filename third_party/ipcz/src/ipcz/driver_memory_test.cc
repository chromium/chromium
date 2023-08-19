// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/driver_memory.h"

#include "ipcz/driver_memory_mapping.h"
#include "test/mock_driver.h"
#include "test/test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "util/ref_counted.h"

namespace ipcz {
namespace {

using testing::_;
using testing::Return;

class DriverMemoryTest : public test::Test {
 public:
  DriverMemoryTest() = default;
  ~DriverMemoryTest() override = default;

  test::MockDriver& driver() { return driver_; }

 private:
  ::testing::StrictMock<test::MockDriver> driver_;
};

TEST_F(DriverMemoryTest, Invalid) {
  DriverMemory memory;
  EXPECT_FALSE(memory.is_valid());
}

TEST_F(DriverMemoryTest, AcquireFromObject) {
  constexpr IpczDriverHandle kHandle = 1234;
  constexpr size_t kSize = 64;
  DriverObject object(test::kMockDriver, kHandle);

  // Constructing a new DriverMemory over a generic DriverObject must query the
  // underlying object for its size.
  EXPECT_CALL(driver(), GetSharedMemoryInfo(kHandle, _, _, _))
      .WillOnce([](IpczDriverHandle handle, uint32_t, const void*,
                   IpczSharedMemoryInfo* info) {
        info->region_num_bytes = kSize;
        return IPCZ_RESULT_OK;
      })
      .RetiresOnSaturation();

  // DriverMemory must reflect the size returned by the driver.
  DriverMemory memory(std::move(object));
  EXPECT_EQ(kSize, memory.size());

  EXPECT_CALL(driver(), Close(kHandle, _, _));
}

TEST_F(DriverMemoryTest, Allocate) {
  constexpr IpczDriverHandle kHandle = 54321;
  constexpr size_t kSize = 256;

  // Constructing a new DriverMemory with a size should allocate a new shared
  // memory region through the driver.
  EXPECT_CALL(driver(), AllocateSharedMemory(kSize, _, _, _))
      .WillOnce([&](size_t num_bytes, uint32_t, const void*,
                    IpczDriverHandle* handle) {
        *handle = kHandle;
        return IPCZ_RESULT_OK;
      })
      .RetiresOnSaturation();

  DriverMemory memory(test::kMockDriver, kSize);
  EXPECT_EQ(kHandle, memory.driver_object().handle());
  EXPECT_EQ(kSize, memory.size());

  EXPECT_CALL(driver(), Close(kHandle, _, _));
}

TEST_F(DriverMemoryTest, Clone) {
  constexpr IpczDriverHandle kHandle = 54321;
  constexpr IpczDriverHandle kDupe = 1234;
  constexpr size_t kSize = 256;

  EXPECT_CALL(driver(), AllocateSharedMemory(kSize, _, _, _))
      .WillOnce([&](size_t num_bytes, uint32_t, const void*,
                    IpczDriverHandle* handle) {
        *handle = kHandle;
        return IPCZ_RESULT_OK;
      })
      .RetiresOnSaturation();

  DriverMemory memory(test::kMockDriver, kSize);

  EXPECT_CALL(driver(), DuplicateSharedMemory(kHandle, _, _, _))
      .WillOnce([&](IpczDriverHandle memory, uint32_t, const void*,
                    IpczDriverHandle* new_handle) {
        *new_handle = kDupe;
        return IPCZ_RESULT_OK;
      })
      .RetiresOnSaturation();
  EXPECT_CALL(driver(), GetSharedMemoryInfo(kDupe, _, _, _))
      .WillOnce([](IpczDriverHandle handle, uint32_t, const void*,
                   IpczSharedMemoryInfo* info) {
        info->region_num_bytes = kSize;
        return IPCZ_RESULT_OK;
      })
      .RetiresOnSaturation();

  DriverMemory clone = memory.Clone();
  EXPECT_TRUE(clone.is_valid());
  EXPECT_EQ(kDupe, clone.driver_object().handle());
  EXPECT_EQ(kSize, clone.size());

  EXPECT_CALL(driver(), Close(kHandle, _, _));
  EXPECT_CALL(driver(), Close(kDupe, _, _));
}

TEST_F(DriverMemoryTest, Map) {
  constexpr IpczDriverHandle kHandle = 424242;
  constexpr IpczDriverHandle kMapping = 777;
  constexpr size_t kSize = 64;

  uint8_t data[64] = {0};
  void* kMappingAddress = &data[0];

  EXPECT_CALL(driver(), AllocateSharedMemory(kSize, _, _, _))
      .WillOnce([&](size_t num_bytes, uint32_t, const void*,
                    IpczDriverHandle* handle) {
        *handle = kHandle;
        return IPCZ_RESULT_OK;
      })
      .RetiresOnSaturation();

  DriverMemory memory(test::kMockDriver, kSize);

  EXPECT_CALL(driver(), MapSharedMemory(kHandle, _, _, _, _))
      .WillOnce([&](IpczDriverHandle memory, uint32_t, const void*,
                    volatile void** addr, IpczDriverHandle* mapping) {
        *addr = kMappingAddress;
        *mapping = kMapping;
        return IPCZ_RESULT_OK;
      })
      .RetiresOnSaturation();

  DriverMemoryMapping mapping = memory.Map();
  EXPECT_TRUE(mapping.is_valid());
  EXPECT_EQ(kMappingAddress, mapping.address());
  EXPECT_EQ(&data[0], mapping.bytes().data());
  EXPECT_EQ(kSize, mapping.bytes().size());

  EXPECT_CALL(driver(), Close(kMapping, _, _));
  EXPECT_CALL(driver(), Close(kHandle, _, _));
}

}  // namespace
}  // namespace ipcz
