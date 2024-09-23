// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/message.h"

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

  DriverTransport::RawMessage AsTransportMessage() { return {data, handles}; }
};

class MessageTest : public testing::Test {
 public:
  MessageTest() {
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
                            uint32_t, const void*, volatile void* data,
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
          static_cast<volatile uint16_t*>(data)[0] =
              static_cast<uint16_t>(handle >> 16);
          handles[0] = handle & 0xffff;
          return IPCZ_RESULT_OK;
        });

    // "Deserialization" reverses the process above: 2 data bytes are expected
    // and 1 transmissible handle is expected, and these are combined into a
    // single new driver handle value to represent the deserialized object.
    EXPECT_CALL(driver(), Deserialize(_, _, _, _, kTransportHandle, _, _, _))
        .WillRepeatedly([&](const volatile void* data, size_t num_bytes,
                            const IpczDriverHandle* handles, size_t num_handles,
                            IpczDriverHandle transport, uint32_t, const void*,
                            IpczDriverHandle* handle) {
          if (reject_driver_objects_) {
            return IPCZ_RESULT_INVALID_ARGUMENT;
          }

          ABSL_ASSERT(num_bytes == 2);
          ABSL_ASSERT(num_handles == 1);
          const uint16_t data_value =
              static_cast<const volatile uint16_t*>(data)[0];
          *handle =
              (static_cast<IpczDriverHandle>(data_value) << 16) | handles[0];
          return IPCZ_RESULT_OK;
        });
  }

  ~MessageTest() override {
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
  const Ref<Node> node_{
      MakeRefCounted<Node>(Node::Type::kNormal, test::kMockDriver)};
  const Ref<DriverTransport> transport_{MakeRefCounted<DriverTransport>(
      DriverObject(test::kMockDriver, kTransportHandle))};
  std::queue<ReceivedMessage> received_messages_;
  bool reject_driver_objects_ = false;
};

TEST_F(MessageTest, BasicMessage) {
  test::msg::BasicTestMessage in;
  EXPECT_GE(sizeof(internal::MessageHeaderV0), in.header().size);
  EXPECT_EQ(0u, in.header().version);
  EXPECT_EQ(test::msg::BasicTestMessage::kId, in.header().message_id);
  EXPECT_EQ(0u, in.header().reserved0[0]);
  EXPECT_EQ(0u, in.header().reserved0[1]);
  EXPECT_EQ(0u, in.header().reserved0[2]);
  EXPECT_EQ(0u, in.header().reserved0[3]);
  EXPECT_EQ(0u, in.header().reserved0[4]);
  EXPECT_EQ(SequenceNumber(0), in.header().sequence_number);
  EXPECT_EQ(0u, in.header().size % 8u);
  EXPECT_EQ(0u, in.v0()->foo);
  EXPECT_EQ(0u, in.v0()->bar);
  EXPECT_EQ(0u, in.header().reserved1);
  in.v0()->foo = 5;
  in.v0()->bar = 7;

  EXPECT_EQ(0u, GetReceivedMessageCount());
  transport().Transmit(in);
  EXPECT_EQ(1u, GetReceivedMessageCount());

  test::msg::BasicTestMessage out;
  ReceivedMessage serialized = TakeNextReceivedMessage();
  EXPECT_TRUE(out.Deserialize(serialized.AsTransportMessage(), transport()));
  EXPECT_EQ(5u, out.v0()->foo);
  EXPECT_EQ(7u, out.v0()->bar);
}

TEST_F(MessageTest, DataArray) {
  test::msg::MessageWithDataArray in;
  in.v0()->values = in.AllocateArray<uint64_t>(3);

  absl::Span<uint64_t> values = in.GetArrayView<uint64_t>(in.v0()->values);
  values[0] = 11;
  values[1] = 13;
  values[2] = 17;

  transport().Transmit(in);

  test::msg::MessageWithDataArray out;
  ReceivedMessage serialized = TakeNextReceivedMessage();
  EXPECT_TRUE(out.Deserialize(serialized.AsTransportMessage(), transport()));

  values = out.GetArrayView<uint64_t>(out.v0()->values);
  ASSERT_EQ(3u, values.size());
  EXPECT_EQ(11u, values[0]);
  EXPECT_EQ(13u, values[1]);
  EXPECT_EQ(17u, values[2]);
}

TEST_F(MessageTest, DriverObject) {
  constexpr IpczDriverHandle kObjectHandle = 0x12345678;

  test::msg::MessageWithDriverObject in;
  in.v0()->object =
      in.AppendDriverObject(DriverObject(test::kMockDriver, kObjectHandle));

  transport().Transmit(in);

  test::msg::MessageWithDriverObject out;
  ReceivedMessage serialized = TakeNextReceivedMessage();
  EXPECT_TRUE(out.Deserialize(serialized.AsTransportMessage(), transport()));

  DriverObject object = out.TakeDriverObject(out.v0()->object);
  EXPECT_EQ(kObjectHandle, object.release());
}

TEST_F(MessageTest, DriverObjectArray) {
  constexpr IpczDriverHandle kObjectHandles[] = {0x12345678, 0x5a5aa5a5,
                                                 0x42425555};
  DriverObject in_objects[std::size(kObjectHandles)];
  for (size_t i = 0; i < std::size(kObjectHandles); ++i) {
    in_objects[i] = DriverObject(test::kMockDriver, kObjectHandles[i]);
  }

  test::msg::MessageWithDriverObjectArray in;
  in.v0()->objects = in.AppendDriverObjects(in_objects);

  transport().Transmit(in);

  test::msg::MessageWithDriverObjectArray out;
  ReceivedMessage serialized = TakeNextReceivedMessage();
  EXPECT_TRUE(out.Deserialize(serialized.AsTransportMessage(), transport()));

  auto objects = out.GetDriverObjectArrayView(out.v0()->objects);
  EXPECT_EQ(3u, objects.size());
  for (size_t i = 0; i < objects.size(); ++i) {
    EXPECT_EQ(kObjectHandles[i], objects[i].release());
  }
}

TEST_F(MessageTest, ShortMessage) {
  test::msg::BasicTestMessage m;
  EXPECT_FALSE(m.Deserialize({m.data_view().subspan(0, 4), {}}, transport()));
}

TEST_F(MessageTest, ShortHeader) {
  test::msg::BasicTestMessage m;
  m.header().size = sizeof(internal::MessageHeaderV0) - 1;
  EXPECT_FALSE(m.Deserialize({m.data_view(), {}}, transport()));
}

TEST_F(MessageTest, HeaderOverflow) {
  test::msg::BasicTestMessage m;
  m.header().size = 255;
  EXPECT_FALSE(m.Deserialize({m.data_view(), {}}, transport()));
}

TEST_F(MessageTest, ShortParamsHeader) {
  test::msg::BasicTestMessage m;
  EXPECT_FALSE(m.Deserialize(
      {m.data_view().subspan(0, sizeof(internal::MessageHeader) + 1), {}},
      transport()));
}

TEST_F(MessageTest, ShortParams) {
  test::msg::BasicTestMessage m;
  m.params().header.size = 1;
  EXPECT_FALSE(m.Deserialize({m.data_view(), {}}, transport()));
}

TEST_F(MessageTest, ParamsOverflow) {
  test::msg::BasicTestMessage m;
  m.params().header.size = 100000;
  EXPECT_FALSE(m.Deserialize({m.data_view(), {}}, transport()));
}

TEST_F(MessageTest, ArrayOffsetOverflow) {
  test::msg::MessageWithDataArray m;
  m.v0()->values = 10000000;
  EXPECT_FALSE(m.Deserialize({m.data_view(), {}}, transport()));
}

TEST_F(MessageTest, ArraySizeOverflow) {
  test::msg::MessageWithDataArray m;
  m.v0()->values = m.AllocateArray<uint64_t>(10);

  auto& header = m.GetValueAt<internal::ArrayHeader>(m.v0()->values);
  header.num_bytes = 1000000;
  EXPECT_FALSE(m.Deserialize({m.data_view(), {}}, transport()));
}

TEST_F(MessageTest, ArrayElementsOverflow) {
  test::msg::MessageWithDataArray m;
  m.v0()->values = m.AllocateArray<uint64_t>(10);

  auto& header = m.GetValueAt<internal::ArrayHeader>(m.v0()->values);
  header.num_elements = 1000000;
  EXPECT_FALSE(m.Deserialize({m.data_view(), {}}, transport()));
}

TEST_F(MessageTest, MalformedDriverObject) {
  constexpr IpczDriverHandle kObjectHandle = 0x12345678;
  test::msg::MessageWithDriverObject in;
  in.v0()->object =
      in.AppendDriverObject(DriverObject(test::kMockDriver, kObjectHandle));

  // Force driver object deserialization to fail. This must result in failure of
  // overall message deserialization.
  set_reject_driver_objects(true);

  transport().Transmit(in);

  ReceivedMessage message = TakeNextReceivedMessage();
  test::msg::MessageWithDriverObject out;
  EXPECT_FALSE(out.Deserialize(message.AsTransportMessage(), transport()));
}

TEST_F(MessageTest, DriverObjectClaimedTwice) {
  // Tests that if a single driver object is claimed more than once by a message
  // parameter, the message is rejected.

  constexpr IpczDriverHandle kObjectHandles[] = {0x12345678, 0x5a5aa5a5,
                                                 0x42425555};
  DriverObject in_objects[std::size(kObjectHandles)];
  for (size_t i = 0; i < std::size(kObjectHandles); ++i) {
    in_objects[i] = DriverObject(test::kMockDriver, kObjectHandles[i]);
  }

  test::msg::MessageWithDriverArrayAndExtraObject in;
  in.v0()->objects = in.AppendDriverObjects(in_objects);

  // Assign the `extra_object` parameter a DriverObject which has already been
  // claimed by the second element of the `objects` parameter.
  in.v0()->extra_object = 1;

  transport().Transmit(in);

  // Although deserialization will fail, it won't fail until parameter
  // validation, after all DriverObjects are deserialized. So we should expect
  // to see clean closure of the attached DriverObjects.
  EXPECT_CALL(driver(), Close(kObjectHandles[0], _, _));
  EXPECT_CALL(driver(), Close(kObjectHandles[1], _, _));
  EXPECT_CALL(driver(), Close(kObjectHandles[2], _, _));

  ReceivedMessage message = TakeNextReceivedMessage();
  test::msg::MessageWithDriverArrayAndExtraObject out;
  EXPECT_FALSE(out.Deserialize(message.AsTransportMessage(), transport()));
}

TEST_F(MessageTest, UnclaimedDriverObjects) {
  // Smoke test to verify that a message with unclaimed DriverObject attachments
  // does not leak.

  constexpr IpczDriverHandle kObjectHandle1 = 0x12345678;
  constexpr IpczDriverHandle kObjectHandle2 = 0xabcdef90;
  constexpr IpczDriverHandle kObjectHandle3 = 0x5a5a5a5a;
  test::msg::MessageWithDriverObject in;
  in.v0()->object =
      in.AppendDriverObject(DriverObject(test::kMockDriver, kObjectHandle1));

  // Append two more objects with no references to them in the message.
  in.AppendDriverObject(DriverObject(test::kMockDriver, kObjectHandle2));
  in.AppendDriverObject(DriverObject(test::kMockDriver, kObjectHandle3));

  transport().Transmit(in);

  ReceivedMessage message = TakeNextReceivedMessage();
  test::msg::MessageWithDriverObject out;
  EXPECT_TRUE(out.Deserialize(message.AsTransportMessage(), transport()));

  // Despite not being claimed by any parameters or otherwise referenced within
  // the message, the extra DriverObjects should be deserialized and now owned
  // by the message object.
  EXPECT_EQ(3u, out.driver_objects().size());
  EXPECT_EQ(kObjectHandle1, out.driver_objects()[0].release());
  EXPECT_EQ(kObjectHandle2, out.driver_objects()[1].release());
  EXPECT_EQ(kObjectHandle3, out.driver_objects()[2].release());
}

TEST_F(MessageTest, AcceptOldVersions) {
  using Msg = test::msg::MessageWithMultipleVersions;
  Msg in;
  in.v0()->a = 2;
  in.v0()->b = 3;
  in.v1()->c = 5;
  in.v1()->d = 7;

  const uint32_t e_offset = in.AllocateArray<uint32_t>(3);
  const auto e_data = in.GetArrayView<uint32_t>(e_offset);
  in.v2()->e = e_offset;
  e_data[0] = 11;
  e_data[1] = 13;
  e_data[2] = 17;

  // Serialize and deserialize the full V2 message.
  {
    transport().Transmit(in);
    ReceivedMessage serialized = TakeNextReceivedMessage();
    Msg out;
    EXPECT_TRUE(out.Deserialize(serialized.AsTransportMessage(), transport()));
    EXPECT_EQ(2u, out.v0()->a);
    EXPECT_EQ(3u, out.v0()->b);
    EXPECT_EQ(5u, out.v1()->c);
    EXPECT_EQ(7u, out.v1()->d);
    const auto data = out.GetArrayView<uint32_t>(out.v2()->e);
    EXPECT_EQ(3u, data.size());
    EXPECT_EQ(11u, data[0]);
    EXPECT_EQ(13u, data[1]);
    EXPECT_EQ(17u, data[2]);
  }

  // Now serialize and deserialize again, forcing the message to look like a
  // V1 message.
  {
    in.params().header.size -= Msg::kVersions[2].size;
    transport().Transmit(in);
    ReceivedMessage serialized = TakeNextReceivedMessage();
    Msg out;
    EXPECT_TRUE(out.Deserialize(serialized.AsTransportMessage(), transport()));
    EXPECT_EQ(2u, out.v0()->a);
    EXPECT_EQ(3u, out.v0()->b);
    EXPECT_EQ(5u, out.v1()->c);
    EXPECT_EQ(7u, out.v1()->d);
    EXPECT_EQ(nullptr, out.v2());
  }

  // Finally, do it for V0.
  {
    in.params().header.size -= Msg::kVersions[1].size;
    transport().Transmit(in);
    ReceivedMessage serialized = TakeNextReceivedMessage();
    Msg out;
    EXPECT_TRUE(out.Deserialize(serialized.AsTransportMessage(), transport()));
    EXPECT_EQ(2u, out.v0()->a);
    EXPECT_EQ(3u, out.v0()->b);
    EXPECT_EQ(nullptr, out.v1());
    EXPECT_EQ(nullptr, out.v2());
  }
}

}  // namespace
}  // namespace ipcz
