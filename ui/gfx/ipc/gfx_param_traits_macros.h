// Copyright 2015 The Chromium Authors. All rights reserved.
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
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/ipc/gfx_ipc_export.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/selection_bound.h"
#include "ui/gfx/swap_result.h"

#if defined(OS_LINUX)
#include "ui/gfx/native_pixmap_handle.h"
#endif

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT GFX_IPC_EXPORT

IPC_ENUM_TRAITS_MAX_VALUE(gfx::GpuMemoryBufferType,
                          gfx::GPU_MEMORY_BUFFER_TYPE_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(gfx::SwapResult, gfx::SwapResult::SWAP_RESULT_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(gfx::SelectionBound::Type, gfx::SelectionBound::LAST)

IPC_ENUM_TRAITS_MAX_VALUE(gfx::GpuFenceHandleType,
                          gfx::GpuFenceHandleType::kLast)

IPC_STRUCT_TRAITS_BEGIN(gfx::CALayerParams)
  IPC_STRUCT_TRAITS_MEMBER(is_empty)
#if defined(OS_MACOSX) && !defined(OS_IOS)
  IPC_STRUCT_TRAITS_MEMBER(ca_context_id)
  IPC_STRUCT_TRAITS_MEMBER(io_surface_mach_port)
  IPC_STRUCT_TRAITS_MEMBER(pixel_size)
  IPC_STRUCT_TRAITS_MEMBER(scale_factor)
#endif
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gfx::GpuMemoryBufferHandle)
  IPC_STRUCT_TRAITS_MEMBER(id)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(region)
  IPC_STRUCT_TRAITS_MEMBER(offset)
  IPC_STRUCT_TRAITS_MEMBER(stride)
#if defined(OS_LINUX) || defined(OS_FUCHSIA)
  IPC_STRUCT_TRAITS_MEMBER(native_pixmap_handle)
#elif defined(OS_MACOSX)
  IPC_STRUCT_TRAITS_MEMBER(mach_port)
#elif defined(OS_WIN)
  IPC_STRUCT_TRAITS_MEMBER(dxgi_handle)
#elif defined(OS_ANDROID)
  IPC_STRUCT_TRAITS_MEMBER(android_hardware_buffer)
#endif
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gfx::GpuMemoryBufferId)
  IPC_STRUCT_TRAITS_MEMBER(id)
IPC_STRUCT_TRAITS_END()

#if defined(OS_LINUX) || defined(OS_FUCHSIA)
IPC_STRUCT_TRAITS_BEGIN(gfx::NativePixmapPlane)
  IPC_STRUCT_TRAITS_MEMBER(stride)
  IPC_STRUCT_TRAITS_MEMBER(offset)
  IPC_STRUCT_TRAITS_MEMBER(size)
#if defined(OS_LINUX)
  IPC_STRUCT_TRAITS_MEMBER(fd)
#elif defined(OS_FUCHSIA)
  IPC_STRUCT_TRAITS_MEMBER(vmo)
#endif
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gfx::NativePixmapHandle)
  IPC_STRUCT_TRAITS_MEMBER(planes)
#if defined(OS_LINUX)
  IPC_STRUCT_TRAITS_MEMBER(modifier)
#endif
#if defined(OS_FUCHSIA)
  IPC_STRUCT_TRAITS_MEMBER(buffer_collection_id)
  IPC_STRUCT_TRAITS_MEMBER(buffer_index)
  IPC_STRUCT_TRAITS_MEMBER(ram_coherency)
#endif
IPC_STRUCT_TRAITS_END()
#endif

IPC_STRUCT_TRAITS_BEGIN(gfx::SwapTimings)
  IPC_STRUCT_TRAITS_MEMBER(swap_start)
  IPC_STRUCT_TRAITS_MEMBER(swap_end)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gfx::SwapResponse)
  IPC_STRUCT_TRAITS_MEMBER(swap_id)
  IPC_STRUCT_TRAITS_MEMBER(result)
  IPC_STRUCT_TRAITS_MEMBER(timings)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gfx::PresentationFeedback)
  IPC_STRUCT_TRAITS_MEMBER(timestamp)
  IPC_STRUCT_TRAITS_MEMBER(interval)
  IPC_STRUCT_TRAITS_MEMBER(flags)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(gfx::GpuFenceHandle)
  IPC_STRUCT_TRAITS_MEMBER(type)
#if defined(OS_POSIX)
  IPC_STRUCT_TRAITS_MEMBER(native_fd)
#endif
IPC_STRUCT_TRAITS_END()

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT

#endif  // UI_GFX_IPC_GFX_PARAM_TRAITS_MACROS_H_
