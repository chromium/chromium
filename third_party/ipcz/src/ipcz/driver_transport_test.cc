// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/driver_transport.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

#include "ipcz/driver_memory.h"
#include "ipcz/driver_memory_mapping.h"
#include "ipcz/driver_object.h"
#include "ipcz/node.h"
#include "ipcz/test_messages.h"
#include "test/mock_driver.h"
#include "test/test_transport_listener.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "util/ref_counted.h"

namespace ipcz {
namespace {

using ::testing::_;
using ::testing::Return;

class DriverTransportTest : public testing::Test {
 public:
  DriverTransportTest() = default;
  ~DriverTransportTest() override = default;

  test::MockDriver& driver() { return driver_; }

  std::pair<Ref<DriverTransport>, Ref<DriverTransport>> CreateTransportPair(
      IpczDriverHandle transport0,
      IpczDriverHandle transport1) {
    return {MakeRefCounted<DriverTransport>(DriverObject(node_, transport0)),
            MakeRefCounted<DriverTransport>(DriverObject(node_, transport1))};
  }

 private:
  ::testing::StrictMock<test::MockDriver> driver_;
  Ref<Node> node_{MakeRefCounted<Node>(Node::Type::kNormal,
                                       test::kMockDriver,
                                       IPCZ_INVALID_DRIVER_HANDLE)};
};

TEST_F(DriverTransportTest, Activation) {
  constexpr IpczDriverHandle kTransport0 = 5;
  constexpr IpczDriverHandle kTransport1 = 42;
  auto [a, b] = CreateTransportPair(kTransport0, kTransport1);

  IpczHandle ipcz_transport = IPCZ_INVALID_HANDLE;
  IpczTransportActivityHandler activity_handler = nullptr;
  EXPECT_CALL(driver(), ActivateTransport(kTransport1, _, _, _, _))
      .WillOnce([&](IpczDriverHandle driver_transport, IpczHandle transport,
                    IpczTransportActivityHandler handler, uint32_t flags,
                    const void* options) {
        ipcz_transport = transport;
        activity_handler = handler;
        return IPCZ_RESULT_OK;
      })
      .RetiresOnSaturation();

  // And verify that the activity handler actually invokes the transport's
  // Listener.

  const std::string kTestMessage = "hihihihi";
  bool received = false;
  test::TestTransportListener listener(b);
  listener.OnStringMessage([&](std::string_view message) {
    EXPECT_EQ(kTestMessage, message);
    received = true;
  });

  // Verify that activation of a DriverTransport feeds the driver an activity
  // handler and valid ipcz handle to use when notifying ipcz of incoming
  // communications.
  EXPECT_NE(IPCZ_INVALID_HANDLE, ipcz_transport);
  EXPECT_TRUE(activity_handler);

  EXPECT_FALSE(received);
  EXPECT_EQ(
      IPCZ_RESULT_OK,
      activity_handler(ipcz_transport, kTestMessage.data(), kTestMessage.size(),
                       nullptr, 0, IPCZ_NO_FLAGS, nullptr));
  EXPECT_TRUE(received);

  // Normal shutdown involves ipcz calling Deactivate() on the DriverTransport.
  // This should result in a call to DeactivateTransport() on the driver.

  EXPECT_CALL(driver(), DeactivateTransport(kTransport1, _, _))
      .WillOnce(Return(IPCZ_RESULT_OK));

  EXPECT_CALL(driver(), Close(kTransport1, _, _));
  EXPECT_CALL(driver(), Close(kTransport0, _, _));

  // The driver must also release its handle to ipcz' DriverTransport, which it
  // does by an invocation of the activity handler like this. Without this, we'd
  // be left with a dangling reference to the DriverTransport.
  EXPECT_EQ(IPCZ_RESULT_OK,
            activity_handler(ipcz_transport, nullptr, 0, nullptr, 0,
                             IPCZ_TRANSPORT_ACTIVITY_DEACTIVATED, nullptr));
}

TEST_F(DriverTransportTest, Error) {
  constexpr IpczDriverHandle kTransport0 = 5;
  constexpr IpczDriverHandle kTransport1 = 42;
  auto [a, b] = CreateTransportPair(kTransport0, kTransport1);

  IpczHandle ipcz_transport = IPCZ_INVALID_HANDLE;
  IpczTransportActivityHandler activity_handler = nullptr;
  EXPECT_CALL(driver(), ActivateTransport(kTransport1, _, _, _, _))
      .WillOnce([&](IpczDriverHandle driver_transport, IpczHandle transport,
                    IpczTransportActivityHandler handler, uint32_t flags,
                    const void* options) {
        ipcz_transport = transport;
        activity_handler = handler;
        return IPCZ_RESULT_OK;
      })
      .RetiresOnSaturation();

  bool observed_error = false;
  test::TestTransportListener listener(b);
  listener.OnError([&] { observed_error = true; });

  // Verify that a driver invoking the activity handler with
  // IPCZ_TRANSPORT_ACTIVITY_ERROR results in an error notification on the
  // DriverTransport's Listener. This implies deactivation on the ipcz side, so
  // no call to Deactivate() is necessary.

  EXPECT_FALSE(observed_error);
  EXPECT_EQ(IPCZ_RESULT_OK,
            activity_handler(ipcz_transport, nullptr, 0, nullptr, 0,
                             IPCZ_TRANSPORT_ACTIVITY_ERROR, nullptr));
  EXPECT_TRUE(observed_error);

  // Even after signaling an error, the driver must also signal deactivation on
  // its side, to release the DriverTransport handle it holds.
  EXPECT_EQ(IPCZ_RESULT_OK,
            activity_handler(ipcz_transport, nullptr, 0, nullptr, 0,
                             IPCZ_TRANSPORT_ACTIVITY_DEACTIVATED, nullptr));

  EXPECT_CALL(driver(), DeactivateTransport(kTransport1, _, _));
  EXPECT_CALL(driver(), Close(kTransport1, _, _));
  EXPECT_CALL(driver(), Close(kTransport0, _, _));
}

TEST_F(DriverTransportTest, Transmit) {
  constexpr IpczDriverHandle kTransport0 = 5;
  constexpr IpczDriverHandle kTransport1 = 42;
  auto [a, b] = CreateTransportPair(kTransport0, kTransport1);

  test::msg::BasicTestMessage message;
  message.params().foo = 5;
  message.params().bar = 7;

  EXPECT_CALL(driver(), Transmit(kTransport0, message.data_view().data(),
                                 message.data_view().size(), _, _, _, _))
      .WillOnce(Return(IPCZ_RESULT_OK));

  a->Transmit(message);

  EXPECT_CALL(driver(), Close(kTransport1, _, _));
  EXPECT_CALL(driver(), Close(kTransport0, _, _));
}

}  // namespace
}  // namespace ipcz
