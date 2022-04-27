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
#include "test/mock_driver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/base/macros.h"
#include "util/ref_counted.h"

namespace ipcz {
namespace {

using ::testing::_;
using ::testing::Return;

DriverTransport::Message MakeMessage(std::string_view s) {
  return DriverTransport::Message(
      absl::MakeSpan(reinterpret_cast<const uint8_t*>(s.data()), s.size()));
}

std::string_view MessageAsString(const DriverTransport::Message& message) {
  return std::string_view(reinterpret_cast<const char*>(message.data.data()),
                          message.data.size());
}

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

class TestListener : public DriverTransport::Listener {
 public:
  using MessageHandler =
      std::function<IpczResult(const DriverTransport::Message&)>;
  using ErrorHandler = std::function<void()>;

  explicit TestListener(MessageHandler message_handler,
                        ErrorHandler error_handler = nullptr)
      : message_handler_(std::move(message_handler)),
        error_handler_(std::move(error_handler)) {}
  ~TestListener() override = default;

  IpczResult OnTransportMessage(
      const DriverTransport::Message& message) override {
    return message_handler_(message);
  }

  void OnTransportError() override {
    if (error_handler_) {
      error_handler_();
    }
  }

 private:
  MessageHandler message_handler_;
  ErrorHandler error_handler_;
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

  // Verify that activation of a DriverTransport feeds the driver an activity
  // handler and valid ipcz handle to use when notifying ipcz of incoming
  // communications.
  b->Activate();
  EXPECT_NE(IPCZ_INVALID_HANDLE, ipcz_transport);
  EXPECT_TRUE(activity_handler);

  // And verify that the activity handler actually invokes the transport's
  // Listener.

  const std::string kTestMessage = "hihihihi";
  bool received = false;
  TestListener listener([&](const DriverTransport::Message& message) {
    EXPECT_EQ(kTestMessage, MessageAsString(message));
    received = true;
    return IPCZ_RESULT_OK;
  });
  b->set_listener(&listener);

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
  b->Deactivate();

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

  b->Activate();

  bool observed_error = false;
  TestListener listener(
      [&](const DriverTransport::Message& message) {
        ABSL_ASSERT(false);
        return IPCZ_RESULT_INVALID_ARGUMENT;
      },
      [&] { observed_error = true; });

  b->set_listener(&listener);

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

  EXPECT_CALL(driver(), Close(kTransport1, _, _));
  EXPECT_CALL(driver(), Close(kTransport0, _, _));
}

TEST_F(DriverTransportTest, Transmit) {
  constexpr IpczDriverHandle kTransport0 = 5;
  constexpr IpczDriverHandle kTransport1 = 42;
  auto [a, b] = CreateTransportPair(kTransport0, kTransport1);

  const std::string kTestMessage = "hihihihi";
  EXPECT_CALL(driver(),
              Transmit(kTransport0, kTestMessage.data(), kTestMessage.size(),
                       nullptr, 0, IPCZ_NO_FLAGS, nullptr))
      .WillOnce(Return(IPCZ_RESULT_OK));

  a->TransmitMessage(MakeMessage(kTestMessage));

  EXPECT_CALL(driver(), Close(kTransport1, _, _));
  EXPECT_CALL(driver(), Close(kTransport0, _, _));
}

}  // namespace
}  // namespace ipcz
