// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/sync_reference_driver.h"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "ipcz/api_object.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ipcz::reference_drivers {
namespace {

struct TransportMessage {
  std::string data;
  std::vector<IpczDriverHandle> handles;
};

using MessageHandler = std::function<IpczResult(TransportMessage message)>;
using DeactivateHandler = std::function<void()>;
using ErrorHandler = std::function<void()>;

struct TransportHandlers {
  MessageHandler on_message = [](TransportMessage message) -> IpczResult {
    return IPCZ_RESULT_OK;
  };
  DeactivateHandler on_deactivate = [] {};
  ErrorHandler on_error = [] {};
};

// This is used by tests to conveniently handle driver transport notifications
// with lambdas.
class TransportReceiver
    : public APIObjectImpl<TransportReceiver, APIObject::kTransportListener> {
 public:
  explicit TransportReceiver(TransportHandlers handlers)
      : handlers_(std::move(handlers)) {}
  ~TransportReceiver() override = default;

  IpczHandle handle() const { return reinterpret_cast<IpczHandle>(this); }

  static IpczResult Receive(IpczHandle transport,
                            const void* data,
                            size_t num_bytes,
                            const IpczDriverHandle* driver_handles,
                            size_t num_driver_handles,
                            IpczTransportActivityFlags flags,
                            const void* options) {
    const TransportHandlers& handlers =
        TransportReceiver::FromHandle(transport)->handlers_;
    if (flags & IPCZ_TRANSPORT_ACTIVITY_DEACTIVATED) {
      handlers.on_deactivate();
      return IPCZ_RESULT_OK;
    }

    if (flags & IPCZ_TRANSPORT_ACTIVITY_ERROR) {
      handlers.on_error();
      return IPCZ_RESULT_OK;
    }

    const std::string message(reinterpret_cast<const char*>(data), num_bytes);
    std::vector<IpczDriverHandle> handles(num_driver_handles);
    std::copy(driver_handles, driver_handles + num_driver_handles,
              handles.begin());
    return handlers.on_message(
        {.data = std::move(message), .handles = std::move(handles)});
  }

  // APIObject:
  IpczResult Close() override { return IPCZ_RESULT_INVALID_ARGUMENT; }

 private:
  const TransportHandlers handlers_;
};

TEST(SyncReferenceDriverTest, CreateTransports) {
  const IpczDriver& driver = kSyncReferenceDriver;
  IpczDriverHandle a, b;
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.CreateTransports(IPCZ_INVALID_DRIVER_HANDLE,
                                    IPCZ_INVALID_DRIVER_HANDLE, IPCZ_NO_FLAGS,
                                    nullptr, &a, &b));
  EXPECT_EQ(IPCZ_RESULT_OK, driver.Close(a, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK, driver.Close(b, IPCZ_NO_FLAGS, nullptr));
}

TEST(SyncReferenceDriverTest, TransmitBeforeActive) {
  const IpczDriver& driver = kSyncReferenceDriver;
  IpczDriverHandle a, b;
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.CreateTransports(IPCZ_INVALID_DRIVER_HANDLE,
                                    IPCZ_INVALID_DRIVER_HANDLE, IPCZ_NO_FLAGS,
                                    nullptr, &a, &b));

  const std::string kMessage = "hello, world?";
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.Transmit(a, kMessage.data(), kMessage.size(), nullptr, 0,
                            IPCZ_NO_FLAGS, nullptr));

  bool received = false;
  TransportReceiver receiver({.on_message = [&](TransportMessage message) {
    EXPECT_EQ(kMessage, message.data);
    received = true;
    return IPCZ_RESULT_OK;
  }});

  // Activation should immediately flush out the already transmitted message.
  EXPECT_FALSE(received);
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.ActivateTransport(b, receiver.handle(),
                                     &TransportReceiver::Receive, IPCZ_NO_FLAGS,
                                     nullptr));
  EXPECT_TRUE(received);

  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.DeactivateTransport(b, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK, driver.Close(a, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK, driver.Close(b, IPCZ_NO_FLAGS, nullptr));
}

TEST(SyncReferenceDriverTest, TransmitWhileActive) {
  const IpczDriver& driver = kSyncReferenceDriver;
  IpczDriverHandle a, b;
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.CreateTransports(IPCZ_INVALID_DRIVER_HANDLE,
                                    IPCZ_INVALID_DRIVER_HANDLE, IPCZ_NO_FLAGS,
                                    nullptr, &a, &b));

  bool received = false;
  const std::string kMessage = "hello, world?";
  TransportReceiver receiver({.on_message = [&](TransportMessage message) {
    EXPECT_EQ(kMessage, message.data);
    received = true;
    return IPCZ_RESULT_OK;
  }});
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.ActivateTransport(b, receiver.handle(),
                                     &TransportReceiver::Receive, IPCZ_NO_FLAGS,
                                     nullptr));

  // Transmission must result in synchronous receipt of the message.
  EXPECT_FALSE(received);
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.Transmit(a, kMessage.data(), kMessage.size(), nullptr, 0,
                            IPCZ_NO_FLAGS, nullptr));
  EXPECT_TRUE(received);

  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.DeactivateTransport(b, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK, driver.Close(a, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK, driver.Close(b, IPCZ_NO_FLAGS, nullptr));
}

TEST(SyncReferenceDriverTest, Deactivate) {
  const IpczDriver& driver = kSyncReferenceDriver;
  IpczDriverHandle a, b;
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.CreateTransports(IPCZ_INVALID_DRIVER_HANDLE,
                                    IPCZ_INVALID_DRIVER_HANDLE, IPCZ_NO_FLAGS,
                                    nullptr, &a, &b));

  bool deactivated = false;
  TransportReceiver receiver({.on_deactivate = [&] { deactivated = true; }});
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.ActivateTransport(b, receiver.handle(),
                                     &TransportReceiver::Receive, IPCZ_NO_FLAGS,
                                     nullptr));

  EXPECT_FALSE(deactivated);
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.DeactivateTransport(b, IPCZ_NO_FLAGS, nullptr));
  EXPECT_TRUE(deactivated);

  EXPECT_EQ(IPCZ_RESULT_OK, driver.Close(a, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK, driver.Close(b, IPCZ_NO_FLAGS, nullptr));
}

TEST(SyncReferenceDriverTest, TransmitAfterDeactivated) {
  const IpczDriver& driver = kSyncReferenceDriver;
  IpczDriverHandle a, b;
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.CreateTransports(IPCZ_INVALID_DRIVER_HANDLE,
                                    IPCZ_INVALID_DRIVER_HANDLE, IPCZ_NO_FLAGS,
                                    nullptr, &a, &b));

  bool message_received = false;
  bool deactivated = false;
  TransportReceiver receiver({
      .on_message =
          [&](TransportMessage message) {
            message_received = true;
            return IPCZ_RESULT_OK;
          },
      .on_deactivate = [&] { deactivated = true; },
  });
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.ActivateTransport(b, receiver.handle(),
                                     &TransportReceiver::Receive, IPCZ_NO_FLAGS,
                                     nullptr));

  EXPECT_FALSE(deactivated);
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.DeactivateTransport(b, IPCZ_NO_FLAGS, nullptr));
  EXPECT_TRUE(deactivated);

  const std::string kMessage = "hello, world?";
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.Transmit(a, kMessage.data(), kMessage.size(), nullptr, 0,
                            IPCZ_NO_FLAGS, nullptr));
  EXPECT_FALSE(message_received);

  EXPECT_EQ(IPCZ_RESULT_OK, driver.Close(a, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK, driver.Close(b, IPCZ_NO_FLAGS, nullptr));
}

TEST(SyncReferenceDriverTest, NotifyError) {
  const IpczDriver& driver = kSyncReferenceDriver;
  IpczDriverHandle a, b;
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.CreateTransports(IPCZ_INVALID_DRIVER_HANDLE,
                                    IPCZ_INVALID_DRIVER_HANDLE, IPCZ_NO_FLAGS,
                                    nullptr, &a, &b));

  bool observed_error = false;
  bool deactivated = false;
  TransportReceiver receiver({
      .on_message =
          [&](TransportMessage m) {
            // Simulate ipcz rejecting an incoming message from the driver.
            return IPCZ_RESULT_INVALID_ARGUMENT;
          },
      .on_deactivate = [&] { deactivated = true; },
      .on_error = [&] { observed_error = true; },
  });
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.ActivateTransport(b, receiver.handle(),
                                     &TransportReceiver::Receive, IPCZ_NO_FLAGS,
                                     nullptr));

  const std::string kMessage = "hello, world?";
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.Transmit(a, kMessage.data(), kMessage.size(), nullptr, 0,
                            IPCZ_NO_FLAGS, nullptr));
  EXPECT_TRUE(observed_error);

  // Errors imply deactivation, so no separate notification happens for
  // deactivation.
  EXPECT_FALSE(deactivated);

  EXPECT_EQ(IPCZ_RESULT_OK, driver.Close(a, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK, driver.Close(b, IPCZ_NO_FLAGS, nullptr));
}

TEST(SyncReferenceDriverTest, SharedMemory) {
  const IpczDriver& driver = kSyncReferenceDriver;

  const size_t kSize = 64;
  IpczDriverHandle memory;
  EXPECT_EQ(IPCZ_RESULT_OK, driver.AllocateSharedMemory(kSize, IPCZ_NO_FLAGS,
                                                        nullptr, &memory));

  IpczSharedMemoryInfo info = {.size = sizeof(info), .region_num_bytes = 0};
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.GetSharedMemoryInfo(memory, IPCZ_NO_FLAGS, nullptr, &info));
  EXPECT_EQ(kSize, info.region_num_bytes);

  IpczDriverHandle dupe;
  EXPECT_EQ(IPCZ_RESULT_OK, driver.DuplicateSharedMemory(memory, IPCZ_NO_FLAGS,
                                                         nullptr, &dupe));
  EXPECT_NE(IPCZ_INVALID_DRIVER_HANDLE, dupe);

  volatile void* addr1;
  IpczDriverHandle mapping1;
  EXPECT_EQ(IPCZ_RESULT_OK, driver.MapSharedMemory(memory, IPCZ_NO_FLAGS,
                                                   nullptr, &addr1, &mapping1));
  EXPECT_NE(IPCZ_INVALID_DRIVER_HANDLE, mapping1);
  EXPECT_NE(nullptr, addr1);

  volatile void* addr2;
  IpczDriverHandle mapping2;
  EXPECT_EQ(IPCZ_RESULT_OK, driver.MapSharedMemory(dupe, IPCZ_NO_FLAGS, nullptr,
                                                   &addr2, &mapping2));
  EXPECT_NE(IPCZ_INVALID_DRIVER_HANDLE, mapping2);
  EXPECT_NE(nullptr, addr2);

  // Two different mappings of the same memory object from the single-process
  // driver will always map to the same base address.
  EXPECT_EQ(addr1, addr2);

  EXPECT_EQ(IPCZ_RESULT_OK, driver.Close(mapping1, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK, driver.Close(mapping2, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK, driver.Close(dupe, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK, driver.Close(memory, IPCZ_NO_FLAGS, nullptr));
}

TEST(SyncReferenceDriverTest, TransmitHandles) {
  const IpczDriver& driver = kSyncReferenceDriver;
  IpczDriverHandle a, b;
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.CreateTransports(IPCZ_INVALID_DRIVER_HANDLE,
                                    IPCZ_INVALID_DRIVER_HANDLE, IPCZ_NO_FLAGS,
                                    nullptr, &a, &b));

  IpczDriverHandle received_handle = IPCZ_INVALID_DRIVER_HANDLE;
  TransportReceiver receiver({.on_message = [&](TransportMessage message) {
    if (message.handles.size() == 1) {
      received_handle = message.handles[0];
    }
    return IPCZ_RESULT_OK;
  }});
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.ActivateTransport(b, receiver.handle(),
                                     &TransportReceiver::Receive, IPCZ_NO_FLAGS,
                                     nullptr));

  IpczDriverHandle memory;
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.AllocateSharedMemory(64, IPCZ_NO_FLAGS, nullptr, &memory));
  EXPECT_NE(IPCZ_INVALID_DRIVER_HANDLE, memory);

  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.Transmit(a, nullptr, 0, &memory, 1, IPCZ_NO_FLAGS, nullptr));

  // Driver handles transmitted through the single-process driver should retain
  // the value across transmission.
  EXPECT_EQ(memory, received_handle);

  EXPECT_EQ(IPCZ_RESULT_OK, driver.Close(memory, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK,
            driver.DeactivateTransport(b, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK, driver.Close(a, IPCZ_NO_FLAGS, nullptr));
  EXPECT_EQ(IPCZ_RESULT_OK, driver.Close(b, IPCZ_NO_FLAGS, nullptr));
}

}  // namespace
}  // namespace ipcz::reference_drivers
