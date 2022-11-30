// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IPC_SKIA_GFX_SKIA_PARAM_TRAITS_MACROS_H_
#define UI_GFX_IPC_SKIA_GFX_SKIA_PARAM_TRAITS_MACROS_H_

#include <stdint.h>

#include "ipc/ipc_message_macros.h"
#include "third_party/skia/include/core/SkImageInfo.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT GFX_SKIA_IPC_EXPORT

IPC_ENUM_TRAITS_MAX_VALUE(SkColorType, kLastEnum_SkColorType)
IPC_ENUM_TRAITS_MAX_VALUE(SkAlphaType, kLastEnum_SkAlphaType)

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT

#endif  // UI_GFX_IPC_SKIA_GFX_SKIA_PARAM_TRAITS_MACROS_H_
