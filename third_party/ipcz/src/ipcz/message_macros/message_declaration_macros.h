// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// no-include-guard-because-multiply-included

#define IPCZ_MSG_BEGIN_INTERFACE(name)
#define IPCZ_MSG_END_INTERFACE()

#define IPCZ_MSG_ID(x) static constexpr uint8_t kId = x
#define IPCZ_MSG_VERSION(x) static constexpr uint32_t kVersion = x

#define IPCZ_MSG_BEGIN(name, id_decl, version_decl)              \
  class name : public MessageWithParams<name##_Params> {         \
   public:                                                       \
    using ParamsType = name##_Params;                            \
    static_assert(sizeof(ParamsType) % 8 == 0, "Invalid size");  \
    id_decl;                                                     \
    version_decl;                                                \
    name();                                                      \
    explicit name(decltype(kIncoming));                          \
    ~name();                                                     \
    bool Deserialize(const DriverTransport::RawMessage& message, \
                     const DriverTransport& transport);          \
    bool DeserializeRelayed(absl::Span<const uint8_t> data,      \
                            absl::Span<DriverObject> objects);   \
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
#define IPCZ_MSG_PARAM_DRIVER_OBJECT_ARRAY(name)            \
  {offsetof(ParamsType, name), sizeof(ParamsType::name), 0, \
   internal::ParamType::kDriverObjectArray},
