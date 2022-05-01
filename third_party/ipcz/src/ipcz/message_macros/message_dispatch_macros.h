// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// no-include-guard-because-multiply-included

#define IPCZ_MSG_ID(x)
#define IPCZ_MSG_VERSION(x)

#define IPCZ_MSG_BEGIN(name, id_decl, version_decl)           \
  case msg::name::kId: {                                      \
    msg::name m;                                              \
    if (m.Deserialize(message, *transport_) && On##name(m)) { \
      return IPCZ_RESULT_OK;                                  \
    }                                                         \
    return IPCZ_RESULT_INVALID_ARGUMENT;                      \
  }

#define IPCZ_MSG_END()

#define IPCZ_MSG_PARAM(type, name)
#define IPCZ_MSG_PARAM_ARRAY(type, name)
#define IPCZ_MSG_PARAM_DRIVER_OBJECT(name)
#define IPCZ_MSG_PARAM_DRIVER_OBJECT_ARRAY(name)
