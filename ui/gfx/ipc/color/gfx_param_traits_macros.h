// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_IPC_COLOR_GFX_PARAM_TRAITS_MACROS_H_
#define UI_GFX_IPC_COLOR_GFX_PARAM_TRAITS_MACROS_H_

#include "ipc/param_traits.h"
#include "ipc/param_traits_macros.h"
#include "ipc/param_traits_utils.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/ipc/color/gfx_ipc_color_export.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT GFX_IPC_COLOR_EXPORT

IPC_ENUM_TRAITS_MAX_VALUE(gfx::ColorSpace::RangeID,
                          gfx::ColorSpace::RangeID::kMaxValue)

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT

#endif  // UI_GFX_IPC_COLOR_GFX_PARAM_TRAITS_MACROS_H_
