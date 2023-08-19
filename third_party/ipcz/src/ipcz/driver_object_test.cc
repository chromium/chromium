// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/driver_object.h"

#include "ipcz/driver_transport.h"
#include "test/mock_driver.h"
#include "test/test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "util/ref_counted.h"

namespace ipcz {
namespace {

using testing::_;
using testing::Return;

class DriverObjectTest : public test::Test {
 public:
  DriverObjectTest() = default;
  ~DriverObjectTest() override = default;

  test::MockDriver& driver() { return driver_; }

 private:
  ::testing::StrictMock<test::MockDriver> driver_;
};

TEST_F(DriverObjectTest, Invalid) {
  DriverObject object;
  EXPECT_FALSE(object.is_valid());
  EXPECT_EQ(nullptr, object.driver());
  EXPECT_EQ(IPCZ_INVALID_DRIVER_HANDLE, object.handle());
  EXPECT_FALSE(object.IsSerializable());

  DriverObject other = std::move(object);
  EXPECT_FALSE(object.is_valid());
  EXPECT_EQ(nullptr, other.driver());
  EXPECT_FALSE(other.is_valid());
  EXPECT_FALSE(other.IsSerializable());
}

TEST_F(DriverObjectTest, Move) {
  constexpr IpczDriverHandle kHandle = 42;
  DriverObject object(test::kMockDriver, kHandle);

  EXPECT_EQ(&test::kMockDriver, object.driver());
  EXPECT_TRUE(object.is_valid());
  EXPECT_EQ(kHandle, object.handle());

  DriverObject other = std::move(object);
  EXPECT_EQ(&test::kMockDriver, other.driver());
  EXPECT_TRUE(other.is_valid());
  EXPECT_EQ(kHandle, other.handle());
  EXPECT_FALSE(object.is_valid());
  EXPECT_EQ(nullptr, object.driver());
  EXPECT_EQ(IPCZ_INVALID_DRIVER_HANDLE, object.handle());

  // Note that releasing prevents DriverObject from invoking the driver's
  // Close() on destruction.
  EXPECT_EQ(kHandle, other.release());
}

TEST_F(DriverObjectTest, Reset) {
  // Both explicit reset and implicit reset (i.e. DriverObject destruction) must
  // invoke Close().

  constexpr IpczDriverHandle kHandle = 5;

  EXPECT_CALL(driver(), Close(kHandle, _, _))
      .WillOnce(Return(IPCZ_RESULT_OK))
      .RetiresOnSaturation();
  { DriverObject object(test::kMockDriver, kHandle); }

  EXPECT_CALL(driver(), Close(kHandle, _, _))
      .WillOnce(Return(IPCZ_RESULT_OK))
      .RetiresOnSaturation();
  {
    DriverObject object(test::kMockDriver, kHandle);
    object.reset();
  }
}

TEST_F(DriverObjectTest, SerializableObject) {
  constexpr IpczDriverHandle kHandle = 5;

  DriverObject object(test::kMockDriver, kHandle);

  EXPECT_CALL(driver(), Serialize(kHandle, _, _, _, _, _, _, _))
      .WillOnce(Return(IPCZ_RESULT_ABORTED))
      .RetiresOnSaturation();
  EXPECT_TRUE(object.IsSerializable());

  EXPECT_CALL(driver(), Serialize(kHandle, _, _, _, _, _, _, _))
      .WillOnce(Return(IPCZ_RESULT_RESOURCE_EXHAUSTED))
      .RetiresOnSaturation();
  EXPECT_TRUE(object.IsSerializable());

  EXPECT_CALL(driver(), Close(kHandle, _, _)).WillOnce(Return(IPCZ_RESULT_OK));
}

TEST_F(DriverObjectTest, UnserializableObject) {
  constexpr IpczDriverHandle kHandle = 5;

  DriverObject object(test::kMockDriver, kHandle);

  EXPECT_CALL(driver(), Serialize(kHandle, _, _, _, _, _, _, _))
      .WillOnce(Return(IPCZ_RESULT_INVALID_ARGUMENT));
  EXPECT_FALSE(object.IsSerializable());

  EXPECT_CALL(driver(), Close(kHandle, _, _)).WillOnce(Return(IPCZ_RESULT_OK));
}

TEST_F(DriverObjectTest, CanTransmit) {
  constexpr IpczDriverHandle kTransport = 42;
  constexpr IpczDriverHandle kHandle = 5;

  auto transport = MakeRefCounted<DriverTransport>(
      DriverObject(test::kMockDriver, kTransport));
  DriverObject object(test::kMockDriver, kHandle);

  EXPECT_CALL(driver(), Serialize(kHandle, kTransport, _, _, _, _, _, _))
      .WillOnce(Return(IPCZ_RESULT_RESOURCE_EXHAUSTED));

  EXPECT_TRUE(object.CanTransmitOn(*transport));

  EXPECT_CALL(driver(), Close(kHandle, _, _)).WillOnce(Return(IPCZ_RESULT_OK));
  EXPECT_CALL(driver(), Close(kTransport, _, _))
      .WillOnce(Return(IPCZ_RESULT_OK));
}

TEST_F(DriverObjectTest, CannotTransmit) {
  constexpr IpczDriverHandle kTransport = 42;
  constexpr IpczDriverHandle kHandle = 5;

  auto transport = MakeRefCounted<DriverTransport>(
      DriverObject(test::kMockDriver, kTransport));
  DriverObject object(test::kMockDriver, kHandle);

  EXPECT_CALL(driver(), Serialize(kHandle, kTransport, _, _, _, _, _, _))
      .WillOnce(Return(IPCZ_RESULT_PERMISSION_DENIED));

  EXPECT_FALSE(object.CanTransmitOn(*transport));

  EXPECT_CALL(driver(), Close(kHandle, _, _)).WillOnce(Return(IPCZ_RESULT_OK));
  EXPECT_CALL(driver(), Close(kTransport, _, _))
      .WillOnce(Return(IPCZ_RESULT_OK));
}

TEST_F(DriverObjectTest, GetSerializedDimensions) {
  constexpr IpczDriverHandle kHandle = 5;
  constexpr IpczDriverHandle kTransport = 42;

  auto transport = MakeRefCounted<DriverTransport>(
      DriverObject(test::kMockDriver, kTransport));
  DriverObject object(test::kMockDriver, kHandle);

  constexpr size_t kNumBytes = 3;
  constexpr size_t kNumHandles = 7;
  EXPECT_CALL(driver(), Serialize(kHandle, kTransport, _, _, _, _, _, _))
      .WillOnce([&](IpczDriverHandle handle, IpczDriverHandle transport,
                    uint32_t flags, const void* options, volatile void* data,
                    size_t* num_bytes, IpczDriverHandle* handles,
                    size_t* num_handles) {
        EXPECT_EQ(nullptr, data);
        EXPECT_EQ(nullptr, handles);
        *num_bytes = kNumBytes;
        *num_handles = kNumHandles;
        return IPCZ_RESULT_RESOURCE_EXHAUSTED;
      })
      .RetiresOnSaturation();

  DriverObject::SerializedDimensions dimensions =
      object.GetSerializedDimensions(*transport);
  EXPECT_EQ(kNumBytes, dimensions.num_bytes);
  EXPECT_EQ(kNumHandles, dimensions.num_driver_handles);

  EXPECT_CALL(driver(), Close(kHandle, _, _)).WillOnce(Return(IPCZ_RESULT_OK));
  EXPECT_CALL(driver(), Close(kTransport, _, _))
      .WillOnce(Return(IPCZ_RESULT_OK));
}

}  // namespace
}  // namespace ipcz
