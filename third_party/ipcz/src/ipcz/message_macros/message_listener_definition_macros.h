// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// no-include-guard-because-multiply-included

#define IPCZ_MSG_BEGIN_INTERFACE(name)                                        \
  bool name##MessageListener::OnMessage(Message& message) {                   \
    return DispatchMessage(message);                                          \
  }                                                                           \
  bool name##MessageListener::OnTransportMessage(                             \
      const DriverTransport::RawMessage& raw_message,                         \
      const DriverTransport& transport) {                                     \
    if (raw_message.data.size() < sizeof(internal::MessageHeaderV0)) {        \
      return false;                                                           \
    }                                                                         \
    const auto& header = *reinterpret_cast<const internal::MessageHeaderV0*>( \
        raw_message.data.data());                                             \
    switch (header.message_id) {
#define IPCZ_MSG_END_INTERFACE()                                     \
  default:                                                           \
    Message message;                                                 \
    return message.DeserializeUnknownType(raw_message, transport) && \
           OnMessage(message);                                       \
    }                                                                \
    }

#define IPCZ_MSG_ID(x)
#define IPCZ_MSG_VERSION(x)

#define IPCZ_MSG_BEGIN(name, id_decl, version_decl)     \
  case name::kId: {                                     \
    name message(Message::kIncoming);                   \
    if (!message.Deserialize(raw_message, transport)) { \
      return false;                                     \
    }                                                   \
    return OnMessage(message);                          \
  }

#define IPCZ_MSG_END()

#define IPCZ_MSG_PARAM(type, name)
#define IPCZ_MSG_PARAM_ARRAY(type, name)
#define IPCZ_MSG_PARAM_DRIVER_OBJECT(name)
#define IPCZ_MSG_PARAM_DRIVER_OBJECT_ARRAY(name)
