// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is generated from node_messages.cc.tmpl and checked-in. Change this
// file by editing the template then running:
//
// node_messages.py --dir={path to *_messages_generator.h}

#include "ipcz/test_messages.h"

#include "third_party/abseil-cpp/absl/types/span.h"

namespace ipcz::test::msg {

// clang-format off

#pragma pack(push, 1)

BasicTestMessage_Params::BasicTestMessage_Params() = default;
BasicTestMessage_Params::~BasicTestMessage_Params() = default;
MessageWithDataArray_Params::MessageWithDataArray_Params() = default;
MessageWithDataArray_Params::~MessageWithDataArray_Params() = default;
MessageWithDriverObject_Params::MessageWithDriverObject_Params() = default;
MessageWithDriverObject_Params::~MessageWithDriverObject_Params() = default;
MessageWithDriverObjectArray_Params::MessageWithDriverObjectArray_Params() = default;
MessageWithDriverObjectArray_Params::~MessageWithDriverObjectArray_Params() = default;
MessageWithDriverArrayAndExtraObject_Params::MessageWithDriverArrayAndExtraObject_Params() = default;
MessageWithDriverArrayAndExtraObject_Params::~MessageWithDriverArrayAndExtraObject_Params() = default;
MessageWithMultipleVersions_Params::MessageWithMultipleVersions_Params() = default;
MessageWithMultipleVersions_Params::~MessageWithMultipleVersions_Params() = default;
MessageWithEnums_Params::MessageWithEnums_Params() = default;
MessageWithEnums_Params::~MessageWithEnums_Params() = default;

BasicTestMessage::BasicTestMessage() = default;
BasicTestMessage::BasicTestMessage(decltype(kIncoming))
    : BasicTestMessage_Base(kIncoming) {}
BasicTestMessage::~BasicTestMessage() = default;

bool BasicTestMessage::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool BasicTestMessage::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata BasicTestMessage_Base::kVersions[];

MessageWithDataArray::MessageWithDataArray() = default;
MessageWithDataArray::MessageWithDataArray(decltype(kIncoming))
    : MessageWithDataArray_Base(kIncoming) {}
MessageWithDataArray::~MessageWithDataArray() = default;

bool MessageWithDataArray::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool MessageWithDataArray::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata MessageWithDataArray_Base::kVersions[];

MessageWithDriverObject::MessageWithDriverObject() = default;
MessageWithDriverObject::MessageWithDriverObject(decltype(kIncoming))
    : MessageWithDriverObject_Base(kIncoming) {}
MessageWithDriverObject::~MessageWithDriverObject() = default;

bool MessageWithDriverObject::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool MessageWithDriverObject::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata MessageWithDriverObject_Base::kVersions[];

MessageWithDriverObjectArray::MessageWithDriverObjectArray() = default;
MessageWithDriverObjectArray::MessageWithDriverObjectArray(decltype(kIncoming))
    : MessageWithDriverObjectArray_Base(kIncoming) {}
MessageWithDriverObjectArray::~MessageWithDriverObjectArray() = default;

bool MessageWithDriverObjectArray::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool MessageWithDriverObjectArray::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata MessageWithDriverObjectArray_Base::kVersions[];

MessageWithDriverArrayAndExtraObject::MessageWithDriverArrayAndExtraObject() = default;
MessageWithDriverArrayAndExtraObject::MessageWithDriverArrayAndExtraObject(decltype(kIncoming))
    : MessageWithDriverArrayAndExtraObject_Base(kIncoming) {}
MessageWithDriverArrayAndExtraObject::~MessageWithDriverArrayAndExtraObject() = default;

bool MessageWithDriverArrayAndExtraObject::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool MessageWithDriverArrayAndExtraObject::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata MessageWithDriverArrayAndExtraObject_Base::kVersions[];

MessageWithMultipleVersions::MessageWithMultipleVersions() = default;
MessageWithMultipleVersions::MessageWithMultipleVersions(decltype(kIncoming))
    : MessageWithMultipleVersions_Base(kIncoming) {}
MessageWithMultipleVersions::~MessageWithMultipleVersions() = default;

bool MessageWithMultipleVersions::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool MessageWithMultipleVersions::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata MessageWithMultipleVersions_Base::kVersions[];

MessageWithEnums::MessageWithEnums() = default;
MessageWithEnums::MessageWithEnums(decltype(kIncoming))
    : MessageWithEnums_Base(kIncoming) {}
MessageWithEnums::~MessageWithEnums() = default;

bool MessageWithEnums::Deserialize(const DriverTransport::RawMessage& message,
                                const DriverTransport& transport) {
  return DeserializeFromTransport(sizeof(ParamsType), absl::MakeSpan(kVersions),
                                  message, transport);
}

bool MessageWithEnums::DeserializeRelayed(absl::Span<const uint8_t> data,
                                       absl::Span<DriverObject> objects) {
  return DeserializeFromRelay(sizeof(ParamsType), absl::MakeSpan(kVersions),
                              data, objects);
}

constexpr internal::VersionMetadata MessageWithEnums_Base::kVersions[];


bool TestMessageListener::OnMessage(Message& message) {
  return DispatchMessage(message);
}

bool TestMessageListener::OnTransportMessage(
    const DriverTransport::RawMessage& raw_message,
    const DriverTransport& transport,
    IpczDriverHandle envelope) {
  if (raw_message.data.size() >= sizeof(internal::MessageHeaderV0)) {
    const auto& header = *reinterpret_cast<const internal::MessageHeaderV0*>(
        raw_message.data.data());
    switch (header.message_id) {
      case BasicTestMessage::kId: {
        BasicTestMessage message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case MessageWithDataArray::kId: {
        MessageWithDataArray message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case MessageWithDriverObject::kId: {
        MessageWithDriverObject message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case MessageWithDriverObjectArray::kId: {
        MessageWithDriverObjectArray message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case MessageWithDriverArrayAndExtraObject::kId: {
        MessageWithDriverArrayAndExtraObject message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case MessageWithMultipleVersions::kId: {
        MessageWithMultipleVersions message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      case MessageWithEnums::kId: {
        MessageWithEnums message(Message::kIncoming);
        message.SetEnvelope(
            DriverObject(*transport.driver_object().driver(), envelope));
        if (!message.Deserialize(raw_message, transport)) {
          return false;
        }
        return OnMessage(message);
      }
      default:
        break;
    }
  }
  Message message;
  message.SetEnvelope(
      DriverObject(*transport.driver_object().driver(), envelope));
  return message.DeserializeUnknownType(raw_message, transport) &&
         OnMessage(message);
}

bool TestMessageListener::DispatchMessage(Message& message) {
  switch (message.header().message_id) {
    case msg::BasicTestMessage::kId:
      return OnBasicTestMessage(static_cast<BasicTestMessage&>(message));
    case msg::MessageWithDataArray::kId:
      return OnMessageWithDataArray(static_cast<MessageWithDataArray&>(message));
    case msg::MessageWithDriverObject::kId:
      return OnMessageWithDriverObject(static_cast<MessageWithDriverObject&>(message));
    case msg::MessageWithDriverObjectArray::kId:
      return OnMessageWithDriverObjectArray(static_cast<MessageWithDriverObjectArray&>(message));
    case msg::MessageWithDriverArrayAndExtraObject::kId:
      return OnMessageWithDriverArrayAndExtraObject(static_cast<MessageWithDriverArrayAndExtraObject&>(message));
    case msg::MessageWithMultipleVersions::kId:
      return OnMessageWithMultipleVersions(static_cast<MessageWithMultipleVersions&>(message));
    case msg::MessageWithEnums::kId:
      return OnMessageWithEnums(static_cast<MessageWithEnums&>(message));
    default:
      // Message might be from a newer version of ipcz so quietly ignore it.
      return true;
  }
}

#pragma pack(pop)

// clang-format on

}  // namespace ipcz::test::msg