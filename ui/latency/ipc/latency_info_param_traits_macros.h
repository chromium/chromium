// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_LATENCY_IPC_LATENCY_INFO_PARAM_TRAITS_MACROS_H_
#define UI_LATENCY_IPC_LATENCY_INFO_PARAM_TRAITS_MACROS_H_

#include "ipc/ipc_message_macros.h"
#include "ui/latency/latency_info.h"

IPC_ENUM_TRAITS_MAX_VALUE(ui::LatencyComponentType,
                          ui::LATENCY_COMPONENT_TYPE_LAST)

#endif  // UI_LATENCY_IPC_LATENCY_INFO_PARAM_TRAITS_MACROS_H_
