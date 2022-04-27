// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// no-include-guard-because-multiply-included

#define IPCZ_MSG_ID(x) static constexpr uint8_t kId = x
#define IPCZ_MSG_VERSION(x) static constexpr uint32_t kVersion = x

#define IPCZ_MSG_BEGIN(name, id_decl, version_decl)      \
  class name : public internal::Message<name##_Params> { \
   public:                                               \
    using ParamsType = name##_Params;                    \
    id_decl;                                             \
    version_decl;                                        \
    name();                                              \
    ~name();                                             \
    bool Serialize(const DriverTransport& transport);    \
    bool Deserialize(const DriverTransport::Message&,    \
                     const DriverTransport& transport);  \
                                                         \
    static constexpr internal::ParamMetadata kMetadata[] = {
#define IPCZ_MSG_END() \
  }                    \
  ;                    \
  }                    \
  ;

#define IPCZ_MSG_PARAM(type, name)                          \
  {offsetof(ParamsType, name), sizeof(ParamsType::name), 0, \
   internal::ParamType::kData},
#define IPCZ_MSG_PARAM_ARRAY(type, name)                               \
  {offsetof(ParamsType, name), sizeof(ParamsType::name), sizeof(type), \
   internal::ParamType::kDataArray},
#define IPCZ_MSG_PARAM_DRIVER_OBJECT(name)                  \
  {offsetof(ParamsType, name), sizeof(ParamsType::name), 0, \
   internal::ParamType::kDriverObject},
#define IPCZ_MSG_PARAM_DRIVER_OBJECT_ARRAY(name)         \
  {offsetof(ParamsType, name), sizeof(ParamsType::name), \
   sizeof(internal::DriverObjectData),                   \
   internal::ParamType::kDriverObjectArray},
