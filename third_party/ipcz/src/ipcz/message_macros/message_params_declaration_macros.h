// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// no-include-guard-because-multiply-included

#define IPCZ_MSG_BEGIN_INTERFACE(name)
#define IPCZ_MSG_END_INTERFACE()

#define IPCZ_MSG_ID(x) static constexpr uint8_t kId = x

#define IPCZ_MSG_BEGIN(name, id_decl)       \
  struct IPCZ_ALIGN(8) name##_Params {      \
    name##_Params();                        \
    ~name##_Params();                       \
    id_decl;                                \
    static constexpr uint32_t kVersion = 0; \
    internal::StructHeader header;

#define IPCZ_MSG_END() \
  }                    \
  ;

#define IPCZ_MSG_BEGIN_VERSION(version)
#define IPCZ_MSG_END_VERSION(version)

#define IPCZ_MSG_PARAM(type, name) type name;
#define IPCZ_MSG_PARAM_ARRAY(type, name) uint32_t name;
#define IPCZ_MSG_PARAM_DRIVER_OBJECT(name) uint32_t name;
#define IPCZ_MSG_PARAM_DRIVER_OBJECT_ARRAY(name) \
  internal::DriverObjectArrayData name;
