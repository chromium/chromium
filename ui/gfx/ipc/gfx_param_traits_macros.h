// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Singly or multiply-included shared traits file depending upon circumstances.
// This allows the use of IPC serialization macros in more than one IPC message
// file.
#ifndef UI_GFX_IPC_GFX_PARAM_TRAITS_MACROS_H_
#define UI_GFX_IPC_GFX_PARAM_TRAITS_MACROS_H_

#include "build/build_config.h"
#include "ipc/ipc_message_macros.h"
#include "ui/gfx/ca_layer_params.h"
#include "ui/gfx/ipc/gfx_ipc_export.h"
#include "ui/gfx/selection_bound.h"
#include "ui/gfx/swap_result.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT GFX_IPC_EXPORT

IPC_ENUM_TRAITS_MAX_VALUE(gfx::SwapResult, gfx::SwapResult::SWAP_RESULT_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(gfx::SelectionBound::Type, gfx::SelectionBound::LAST)

IPC_STRUCT_TRAITS_BEGIN(gfx::CALayerParams)
  IPC_STRUCT_TRAITS_MEMBER(is_empty)
#if BUILDFLAG(IS_MAC)
  IPC_STRUCT_TRAITS_MEMBER(ca_context_id)
  IPC_STRUCT_TRAITS_MEMBER(io_surface_mach_port)
  IPC_STRUCT_TRAITS_MEMBER(pixel_size)
  IPC_STRUCT_TRAITS_MEMBER(scale_factor)
#endif
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gfx::SwapTimings)
  IPC_STRUCT_TRAITS_MEMBER(swap_start)
  IPC_STRUCT_TRAITS_MEMBER(swap_end)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gfx::SwapResponse)
  IPC_STRUCT_TRAITS_MEMBER(swap_id)
  IPC_STRUCT_TRAITS_MEMBER(result)
  IPC_STRUCT_TRAITS_MEMBER(timings)
IPC_STRUCT_TRAITS_END()

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT

#endif  // UI_GFX_IPC_GFX_PARAM_TRAITS_MACROS_H_
