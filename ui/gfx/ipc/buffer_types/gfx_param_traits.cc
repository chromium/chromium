// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ipc/buffer_types/gfx_param_traits.h"

// Generate param traits write methods.
#include "ipc/param_traits_write_macros.h"
namespace IPC {
#undef UI_GFX_IPC_BUFFER_TYPES_GFX_PARAM_TRAITS_MACROS_H_
#include "ui/gfx/ipc/buffer_types/gfx_param_traits_macros.h"
}  // namespace IPC

// Generate param traits read methods.
#include "ipc/param_traits_read_macros.h"
namespace IPC {
#undef UI_GFX_IPC_BUFFER_TYPES_GFX_PARAM_TRAITS_MACROS_H_
#include "ui/gfx/ipc/buffer_types/gfx_param_traits_macros.h"
}  // namespace IPC
