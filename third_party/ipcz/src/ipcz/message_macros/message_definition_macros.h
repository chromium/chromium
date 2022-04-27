// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// no-include-guard-because-multiply-included

#define IPCZ_MSG_ID(x)
#define IPCZ_MSG_VERSION(x)

#define IPCZ_MSG_BEGIN(name, id_decl, version_decl)                          \
  name::name() = default;                                                    \
  name::~name() = default;                                                   \
  bool name::Serialize(const DriverTransport& transport) {                   \
    if (!CanTransmitOn(transport)) {                                         \
      return false;                                                          \
    }                                                                        \
    MessageBase::Serialize(kMetadata, transport);                            \
    return true;                                                             \
  }                                                                          \
  bool name::Deserialize(const DriverTransport::Message& message,            \
                         const DriverTransport& transport) {                 \
    return DeserializeFromTransport(sizeof(ParamsType), kVersion,            \
                                    absl::MakeSpan(kMetadata), message.data, \
                                    message.handles, transport);             \
  }                                                                          \
  constexpr internal::ParamMetadata name::kMetadata[];

#define IPCZ_MSG_END()

#define IPCZ_MSG_PARAM(type, name)
#define IPCZ_MSG_PARAM_ARRAY(type, name)
#define IPCZ_MSG_PARAM_DRIVER_OBJECT(name)
#define IPCZ_MSG_PARAM_DRIVER_OBJECT_ARRAY(name)
