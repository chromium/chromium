// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "reference_drivers/socket_transport.h"

#include <string_view>
#include <tuple>
#include <vector>

#include "build/build_config.h"
#include "reference_drivers/file_descriptor.h"
#include "reference_drivers/memfd_memory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/synchronization/notification.h"

namespace ipcz::reference_drivers {
namespace {

using SocketTransportTest = testing::Test;

using testing::ElementsAreArray;

void DeactivateSync(SocketTransport& transport) {
  absl::Notification notification;
  transport.Deactivate([&notification] { notification.Notify(); });
  notification.WaitForNotification();
}

const char kTestMessage1[] = "Hello, world!";

absl::Span<const uint8_t> AsBytes(std::string_view str) {
  return absl::MakeSpan(reinterpret_cast<const uint8_t*>(str.data()),
                        str.size());
}

std::string_view AsString(absl::Span<const uint8_t> bytes) {
  return std::string_view(reinterpret_cast<const char*>(bytes.data()),
                          bytes.size());
}

TEST_F(SocketTransportTest, ReadWrite) {
  auto [a, b] = SocketTransport::CreatePair();

  absl::Notification b_finished;
  b->Activate([&b_finished](SocketTransport::Message message) {
    EXPECT_EQ(kTestMessage1, AsString(message.data));
    b_finished.Notify();
    return true;
  });

  a->Send({.data = AsBytes(kTestMessage1)});

  b_finished.WaitForNotification();
  DeactivateSync(*b);
}

TEST_F(SocketTransportTest, Disconnect) {
  auto [a, b] = SocketTransport::CreatePair();

  bool received_message = false;
  absl::Notification b_finished;
  b->Activate(
      [&received_message](SocketTransport::Message message) {
        received_message = true;
        return true;
      },
      [&b_finished] { b_finished.Notify(); });

  a.reset();

  b_finished.WaitForNotification();
  DeactivateSync(*b);

  EXPECT_FALSE(received_message);
}

TEST_F(SocketTransportTest, Flood) {
  // Smoke test to throw very large number of messages at a SocketTransport, to
  // exercise any queueing behavior that might be implemented.
  constexpr size_t kNumMessages = 25000;

  // Every message sent is filled with this many uint32 values, all reflecting
  // the index of the message within the sequence. So the first message is
  // filled with 0x00000000, the second is filled with 0x00000001, etc.
  constexpr size_t kMessageNumValues = 256;
  constexpr size_t kMessageNumBytes = kMessageNumValues * sizeof(uint32_t);

  auto [a, b] = SocketTransport::CreatePair();

  uint32_t next_expected_value = 0;
  std::vector<uint32_t> expected_values(kMessageNumValues);
  absl::Span<uint8_t> expected_bytes = absl::MakeSpan(
      reinterpret_cast<uint8_t*>(expected_values.data()), kMessageNumBytes);

  absl::Notification b_finished;
  a->Activate();
  b->Activate([&](SocketTransport::Message message) {
    EXPECT_EQ(kMessageNumBytes, message.data.size());

    // Make sure messages arrive in the order they were sent.
    std::fill(expected_values.begin(), expected_values.end(),
              next_expected_value++);
    EXPECT_EQ(0, memcmp(message.data.data(), expected_bytes.data(),
                        kMessageNumBytes));

    // Finish only once the last expected message is received.
    if (next_expected_value == kNumMessages) {
      b_finished.Notify();
    }
    return true;
  });

  // Spam, spam, spam, spam, spam.
  for (size_t i = 0; i < kNumMessages; ++i) {
    std::vector<uint32_t> message(kMessageNumValues);
    std::fill(message.begin(), message.end(), static_cast<uint32_t>(i));
    a->Send({.data = absl::MakeSpan(reinterpret_cast<uint8_t*>(message.data()),
                                    kMessageNumBytes)});
  }

  b_finished.WaitForNotification();
  DeactivateSync(*b);
  DeactivateSync(*a);
}

TEST_F(SocketTransportTest, DestroyFromIOThread) {
  auto channels = SocketTransport::CreatePair();
  Ref<SocketTransport> a = std::move(channels.first);
  Ref<SocketTransport> b = std::move(channels.second);

  absl::Notification destruction_done;
  b->Activate([](SocketTransport::Message message) { return true; },
              [&b, done = &destruction_done] {
                b->Deactivate([done] { done->Notify(); });
                b.reset();
              });

  // Closing `a` should elicit `b` invoking the above error handler on b's I/O
  // thread.
  a.reset();

  destruction_done.WaitForNotification();
}

TEST_F(SocketTransportTest, SerializeAndDeserialize) {
  // Basic smoke test to verify that a SocketTransport can be decomposed into
  // its underlying socket descriptor and then reconstructed from that.
  auto [a, b] = SocketTransport::CreatePair();

  FileDescriptor fd = b->TakeDescriptor();
  b.reset();

  b = MakeRefCounted<SocketTransport>(std::move(fd));

  absl::Notification b_finished;
  b->Activate([&b_finished](SocketTransport::Message message) {
    EXPECT_EQ(kTestMessage1, AsString(message.data));
    b_finished.Notify();
    return true;
  });

  a->Send({.data = AsBytes(kTestMessage1)});

  b_finished.WaitForNotification();
  DeactivateSync(*b);
}

TEST_F(SocketTransportTest, ReadWriteWithFileDescriptor) {
  auto [a, b] = SocketTransport::CreatePair();

  static const std::string_view kMemoryMessage = "heckin memory chonk here";
  MemfdMemory memory(kMemoryMessage.size());
  MemfdMemory::Mapping mapping = memory.Map();
  std::copy(kMemoryMessage.begin(), kMemoryMessage.end(),
            mapping.bytes().begin());

  absl::Notification b_finished;
  b->Activate([&b_finished](SocketTransport::Message message) {
    EXPECT_EQ(kTestMessage1, AsString(message.data));
    [&] { ASSERT_EQ(1u, message.descriptors.size()); }();

    MemfdMemory memory(std::move(message.descriptors[0]),
                       kMemoryMessage.size());
    MemfdMemory::Mapping mapping = memory.Map();
    EXPECT_THAT(mapping.bytes(), ElementsAreArray(kMemoryMessage));
    b_finished.Notify();
    return true;
  });

  FileDescriptor memory_fd = memory.TakeDescriptor();
  a->Send({.data = AsBytes(kTestMessage1), .descriptors = {&memory_fd, 1}});

  b_finished.WaitForNotification();
  DeactivateSync(*b);
}

}  // namespace
}  // namespace ipcz::reference_drivers
