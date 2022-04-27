// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// no-include-guard-because-multiply-included

IPCZ_MSG_BEGIN(BasicTestMessage, IPCZ_MSG_ID(0), IPCZ_MSG_VERSION(0))
  IPCZ_MSG_PARAM(uint32_t, foo)
  IPCZ_MSG_PARAM(uint32_t, bar)
IPCZ_MSG_END()

IPCZ_MSG_BEGIN(BasicTestMessageV1, IPCZ_MSG_ID(0), IPCZ_MSG_VERSION(1))
  IPCZ_MSG_PARAM(uint32_t, foo)
  IPCZ_MSG_PARAM(uint32_t, bar)
  IPCZ_MSG_PARAM(uint32_t, baz)
  IPCZ_MSG_PARAM(uint32_t, qux)
IPCZ_MSG_END()

IPCZ_MSG_BEGIN(MessageWithDataArray, IPCZ_MSG_ID(1), IPCZ_MSG_VERSION(0))
  IPCZ_MSG_PARAM_ARRAY(uint64_t, values)
IPCZ_MSG_END()

IPCZ_MSG_BEGIN(MessageWithDriverObject, IPCZ_MSG_ID(2), IPCZ_MSG_VERSION(0))
  IPCZ_MSG_PARAM_DRIVER_OBJECT(object)
IPCZ_MSG_END()

IPCZ_MSG_BEGIN(MessageWithDriverObjectArray,
               IPCZ_MSG_ID(3),
               IPCZ_MSG_VERSION(0))
  IPCZ_MSG_PARAM_DRIVER_OBJECT_ARRAY(objects)
IPCZ_MSG_END()
