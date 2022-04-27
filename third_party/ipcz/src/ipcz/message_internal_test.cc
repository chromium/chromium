// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/message_internal.h"

#include <cstdint>
#include <queue>
#include <utility>
#include <vector>

#include "ipcz/driver_object.h"
#include "ipcz/driver_transport.h"
#include "ipcz/ipcz.h"
#include "ipcz/node.h"
#include "ipcz/test_messages.h"
#include "test/mock_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "util/ref_counted.h"

namespace ipcz {
namespace {

using testing::_;

constexpr IpczDriverHandle kTransportHandle = 42;

// Structure used to temporarily store messages transmitted through a transport,
// so that tests can inspect and/or deserialize them later.
struct ReceivedMessage {
  std::vector<uint8_t> data;
  std::vector<IpczDriverHandle> handles;

  DriverTransport::Message AsTransportMessage() {
    return DriverTransport::Message(absl::MakeSpan(data),
                                    absl::MakeSpan(handles));
  }
};

class MessageInternalTest : public testing::Test {
 public:
  MessageInternalTest() {
    // All serialized messages transmitted through `transport()` will be
    // captured directly in `received_messages_`.
    EXPECT_CALL(driver(), Transmit(kTransportHandle, _, _, _, _, _, _))
        .WillRepeatedly([&](IpczDriverHandle driver_transport, const void* data,
                            size_t num_bytes, const IpczDriverHandle* handles,
                            size_t num_handles, uint32_t, const void*) {
          const uint8_t* bytes = static_cast<const uint8_t*>(data);
          received_messages_.push(
              {{bytes, bytes + num_bytes}, {handles, handles + num_handles}});
          return IPCZ_RESULT_OK;
        });

    // For convenient automation when exercising DriverObject transmission, all
    // driver handles in these tests are treated as 32-bit values. Their
    // "serialized" form is the same value decomposed: the high 16-bits are the
    // serialized data bytes, and the low 16-bits are treated as a new
    // transmissible driver handle.
    EXPECT_CALL(driver(), Serialize(_, kTransportHandle, _, _, _, _, _, _))
        .WillRepeatedly([&](IpczDriverHandle handle, IpczDriverHandle transport,
                            uint32_t, const void*, void* data,
                            size_t* num_bytes, IpczDriverHandle* handles,
                            size_t* num_handles) {
          const size_t data_capacity = num_bytes ? *num_bytes : 0;
          const size_t handle_capacity = num_handles ? *num_handles : 0;
          if (num_bytes) {
            *num_bytes = 2;
          }
          if (num_handles) {
            *num_handles = 1;
          }
          if (!data || !handles || data_capacity < 2 || handle_capacity < 1) {
            return IPCZ_RESULT_RESOURCE_EXHAUSTED;
          }
          static_cast<uint16_t*>(data)[0] = static_cast<uint16_t>(handle >> 16);
          handles[0] = handle & 0xffff;
          return IPCZ_RESULT_OK;
        });

    // "Deserialization" reverses the process above: 2 data bytes are expected
    // and 1 transmissible handle is expected, and these are combined into a
    // single new driver handle value to represent the deserialized object.
    EXPECT_CALL(driver(), Deserialize(_, _, _, _, kTransportHandle, _, _, _))
        .WillRepeatedly([&](const void* data, size_t num_bytes,
                            const IpczDriverHandle* handles, size_t num_handles,
                            IpczDriverHandle transport, uint32_t, const void*,
                            IpczDriverHandle* handle) {
          if (reject_driver_objects_) {
            return IPCZ_RESULT_INVALID_ARGUMENT;
          }

          ABSL_ASSERT(num_bytes == 2);
          ABSL_ASSERT(num_handles == 1);
          const uint16_t data_value = static_cast<const uint16_t*>(data)[0];
          *handle =
              (static_cast<IpczDriverHandle>(data_value) << 16) | handles[0];
          return IPCZ_RESULT_OK;
        });
  }

  ~MessageInternalTest() override {
    EXPECT_CALL(driver_, Close(kTransportHandle, _, _));
  }

  test::MockDriver& driver() { return driver_; }
  const Ref<Node>& node() const { return node_; }
  DriverTransport& transport() { return *transport_; }

  void set_reject_driver_objects(bool reject) {
    reject_driver_objects_ = reject;
  }

  size_t GetReceivedMessageCount() const { return received_messages_.size(); }

  ReceivedMessage TakeNextReceivedMessage() {
    ABSL_ASSERT(!received_messages_.empty());
    ReceivedMessage message = std::move(received_messages_.front());
    received_messages_.pop();
    return message;
  }

 private:
  ::testing::StrictMock<test::MockDriver> driver_;
  const Ref<Node> node_{MakeRefCounted<Node>(Node::Type::kNormal,
                                             test::kMockDriver,
                                             IPCZ_INVALID_DRIVER_HANDLE)};
  const Ref<DriverTransport> transport_{
      MakeRefCounted<DriverTransport>(DriverObject(node_, kTransportHandle))};
  std::queue<ReceivedMessage> received_messages_;
  bool reject_driver_objects_ = false;
};

TEST_F(MessageInternalTest, BasicMessage) {
  test::msg::BasicTestMessage in;
  EXPECT_GE(sizeof(internal::MessageHeaderV0), in.header().size);
  EXPECT_EQ(0u, in.header().version);
  EXPECT_EQ(test::msg::BasicTestMessage::kId, in.header().message_id);
  EXPECT_EQ(0u, in.header().reserved[0]);
  EXPECT_EQ(0u, in.header().reserved[1]);
  EXPECT_EQ(0u, in.header().reserved[2]);
  EXPECT_EQ(0u, in.header().reserved[3]);
  EXPECT_EQ(0u, in.header().reserved[4]);
  EXPECT_EQ(SequenceNumber(0), in.header().sequence_number);
  EXPECT_EQ(0u, in.header().size % 8u);
  EXPECT_EQ(0u, in.params().foo);
  EXPECT_EQ(0u, in.params().bar);
  in.params().foo = 5;
  in.params().bar = 7;

  EXPECT_EQ(0u, GetReceivedMessageCount());
  transport().Transmit(in);
  EXPECT_EQ(1u, GetReceivedMessageCount());

  test::msg::BasicTestMessage out;
  ReceivedMessage serialized = TakeNextReceivedMessage();
  EXPECT_TRUE(out.Deserialize(serialized.AsTransportMessage(), transport()));
  EXPECT_EQ(5u, out.params().foo);
  EXPECT_EQ(7u, out.params().bar);
}

TEST_F(MessageInternalTest, DataArray) {
  test::msg::MessageWithDataArray in;
  in.params().values = in.AllocateArray<uint64_t>(3);

  absl::Span<uint64_t> values = in.GetArrayView<uint64_t>(in.params().values);
  values[0] = 11;
  values[1] = 13;
  values[2] = 17;

  transport().Transmit(in);

  test::msg::MessageWithDataArray out;
  ReceivedMessage serialized = TakeNextReceivedMessage();
  EXPECT_TRUE(out.Deserialize(serialized.AsTransportMessage(), transport()));

  values = out.GetArrayView<uint64_t>(out.params().values);
  ASSERT_EQ(3u, values.size());
  EXPECT_EQ(11u, values[0]);
  EXPECT_EQ(13u, values[1]);
  EXPECT_EQ(17u, values[2]);
}

TEST_F(MessageInternalTest, DriverObject) {
  constexpr IpczDriverHandle kObjectHandle = 0x12345678;

  test::msg::MessageWithDriverObject in;
  in.AppendDriverObject(DriverObject(node(), kObjectHandle),
                        in.params().object);

  transport().Transmit(in);

  test::msg::MessageWithDriverObject out;
  ReceivedMessage serialized = TakeNextReceivedMessage();
  EXPECT_TRUE(out.Deserialize(serialized.AsTransportMessage(), transport()));

  DriverObject object = out.TakeDriverObject(out.params().object);
  EXPECT_EQ(kObjectHandle, object.release());
}

TEST_F(MessageInternalTest, DriverObjectArray) {
  constexpr IpczDriverHandle kObjectHandles[] = {0x12345678, 0x5a5aa5a5,
                                                 0x42425555};
  DriverObject in_objects[std::size(kObjectHandles)];
  for (size_t i = 0; i < std::size(kObjectHandles); ++i) {
    in_objects[i] = DriverObject(node(), kObjectHandles[i]);
  }

  test::msg::MessageWithDriverObjectArray in;
  in.params().objects = in.AppendDriverObjects(in_objects);

  transport().Transmit(in);

  test::msg::MessageWithDriverObjectArray out;
  ReceivedMessage serialized = TakeNextReceivedMessage();
  EXPECT_TRUE(out.Deserialize(serialized.AsTransportMessage(), transport()));

  auto objects_data =
      out.GetArrayView<internal::DriverObjectData>(out.params().objects);
  EXPECT_EQ(3u, objects_data.size());
  for (size_t i = 0; i < objects_data.size(); ++i) {
    EXPECT_EQ(kObjectHandles[i],
              out.TakeDriverObject(objects_data[i]).release());
  }
}

TEST_F(MessageInternalTest, ShortMessage) {
  test::msg::BasicTestMessage m;
  EXPECT_FALSE(m.Deserialize(
      DriverTransport::Message(m.data_view().subspan(0, 4)), transport()));
}

TEST_F(MessageInternalTest, ShortHeader) {
  test::msg::BasicTestMessage m;
  m.header().size = sizeof(internal::MessageHeaderV0) - 1;
  EXPECT_FALSE(
      m.Deserialize(DriverTransport::Message(m.data_view()), transport()));
}

TEST_F(MessageInternalTest, HeaderOverflow) {
  test::msg::BasicTestMessage m;
  m.header().size = 255;
  EXPECT_FALSE(
      m.Deserialize(DriverTransport::Message(m.data_view()), transport()));
}

TEST_F(MessageInternalTest, ShortParamsHeader) {
  test::msg::BasicTestMessage m;
  EXPECT_FALSE(m.Deserialize(DriverTransport::Message(m.data_view().subspan(
                                 0, sizeof(internal::MessageHeader) + 1)),
                             transport()));
}

TEST_F(MessageInternalTest, ShortPrams) {
  test::msg::BasicTestMessage m;
  m.params().header.size = 1;
  EXPECT_FALSE(
      m.Deserialize(DriverTransport::Message(m.data_view()), transport()));
}

TEST_F(MessageInternalTest, ParamsOverflow) {
  test::msg::BasicTestMessage m;
  m.params().header.size = 100000;
  EXPECT_FALSE(
      m.Deserialize(DriverTransport::Message(m.data_view()), transport()));
}

TEST_F(MessageInternalTest, ArrayOffsetOverflow) {
  test::msg::MessageWithDataArray m;
  m.params().values = 10000000;
  EXPECT_FALSE(
      m.Deserialize(DriverTransport::Message(m.data_view()), transport()));
}

TEST_F(MessageInternalTest, ArraySizeOverflow) {
  test::msg::MessageWithDataArray m;
  m.params().values = m.AllocateArray<uint64_t>(10);

  auto& header = m.GetValueAt<internal::ArrayHeader>(m.params().values);
  header.num_bytes = 1000000;
  EXPECT_FALSE(
      m.Deserialize(DriverTransport::Message(m.data_view()), transport()));
}

TEST_F(MessageInternalTest, ArrayElementsOverflow) {
  test::msg::MessageWithDataArray m;
  m.params().values = m.AllocateArray<uint64_t>(10);

  auto& header = m.GetValueAt<internal::ArrayHeader>(m.params().values);
  header.num_elements = 1000000;
  EXPECT_FALSE(
      m.Deserialize(DriverTransport::Message(m.data_view()), transport()));
}

TEST_F(MessageInternalTest, MalformedDriverObject) {
  constexpr IpczDriverHandle kObjectHandle = 0x12345678;
  test::msg::MessageWithDriverObject in;
  in.AppendDriverObject(DriverObject(node(), kObjectHandle),
                        in.params().object);
  transport().Transmit(in);

  // Force driver object deserialization to fail. This must result in failure of
  // overall message deserialization.
  set_reject_driver_objects(true);
  ReceivedMessage message = TakeNextReceivedMessage();
  test::msg::MessageWithDriverObject out;
  EXPECT_FALSE(out.Deserialize(message.AsTransportMessage(), transport()));
}

TEST_F(MessageInternalTest, TolerateNewerVersion) {
  test::msg::BasicTestMessageV1 in;
  in.params().foo = 1;
  in.params().bar = 2;
  in.params().baz = 3;
  in.params().qux = 4;
  transport().Transmit(in);

  test::msg::BasicTestMessage out;
  ReceivedMessage message = TakeNextReceivedMessage();
  EXPECT_TRUE(out.Deserialize(message.AsTransportMessage(), transport()));
  EXPECT_EQ(1u, out.params().foo);
  EXPECT_EQ(2u, out.params().bar);
}

}  // namespace
}  // namespace ipcz
