// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Multiply-included message file, hence no include guard here, but see below
// for a much smaller-than-usual include guard section.
// no-include-guard-because-multiply-included

#include <stdint.h>

#include <vector>

#include "base/file_descriptor_posix.h"
#include "ipc/ipc_message_macros.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/gamma_ramp_rgb_entry.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/ipc/buffer_types/gfx_param_traits.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"
#include "ui/gfx/ipc/geometry/gfx_param_traits.h"
#include "ui/gfx/ipc/gfx_param_traits.h"
#include "ui/gfx/ipc/skia/gfx_skia_param_traits.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/common/gpu/ozone_gpu_message_params.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT

#define IPC_MESSAGE_START OzoneGpuMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(display::DisplayConnectionType,
                          display::DISPLAY_CONNECTION_TYPE_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(display::HDCPState, display::HDCP_STATE_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(display::PanelOrientation,
                          display::PanelOrientation::kLast)

IPC_ENUM_TRAITS_MAX_VALUE(gfx::OverlayTransform, gfx::OVERLAY_TRANSFORM_LAST)

IPC_ENUM_TRAITS_MAX_VALUE(ui::OverlayStatus, ui::OVERLAY_STATUS_LAST)

// clang-format off
IPC_STRUCT_TRAITS_BEGIN(ui::DisplayMode_Params)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(is_interlaced)
  IPC_STRUCT_TRAITS_MEMBER(refresh_rate)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ui::DisplaySnapshot_Params)
  IPC_STRUCT_TRAITS_MEMBER(display_id)
  IPC_STRUCT_TRAITS_MEMBER(origin)
  IPC_STRUCT_TRAITS_MEMBER(physical_size)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(is_aspect_preserving_scaling)
  IPC_STRUCT_TRAITS_MEMBER(has_overscan)
  IPC_STRUCT_TRAITS_MEMBER(has_color_correction_matrix)
  IPC_STRUCT_TRAITS_MEMBER(color_correction_in_linear_space)
  IPC_STRUCT_TRAITS_MEMBER(color_space)
  IPC_STRUCT_TRAITS_MEMBER(bits_per_channel)
  IPC_STRUCT_TRAITS_MEMBER(display_name)
  IPC_STRUCT_TRAITS_MEMBER(sys_path)
  IPC_STRUCT_TRAITS_MEMBER(modes)
  IPC_STRUCT_TRAITS_MEMBER(panel_orientation)
  IPC_STRUCT_TRAITS_MEMBER(edid)
  IPC_STRUCT_TRAITS_MEMBER(has_current_mode)
  IPC_STRUCT_TRAITS_MEMBER(current_mode)
  IPC_STRUCT_TRAITS_MEMBER(has_native_mode)
  IPC_STRUCT_TRAITS_MEMBER(native_mode)
  IPC_STRUCT_TRAITS_MEMBER(product_code)
  IPC_STRUCT_TRAITS_MEMBER(year_of_manufacture)
  IPC_STRUCT_TRAITS_MEMBER(maximum_cursor_size)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(display::GammaRampRGBEntry)
  IPC_STRUCT_TRAITS_MEMBER(r)
  IPC_STRUCT_TRAITS_MEMBER(g)
  IPC_STRUCT_TRAITS_MEMBER(b)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ui::OverlayCheck_Params)
  IPC_STRUCT_TRAITS_MEMBER(buffer_size)
  IPC_STRUCT_TRAITS_MEMBER(transform)
  IPC_STRUCT_TRAITS_MEMBER(format)
  IPC_STRUCT_TRAITS_MEMBER(display_rect)
  IPC_STRUCT_TRAITS_MEMBER(crop_rect)
  IPC_STRUCT_TRAITS_MEMBER(is_opaque)
  IPC_STRUCT_TRAITS_MEMBER(plane_z_order)
  IPC_STRUCT_TRAITS_MEMBER(is_overlay_candidate)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(ui::OverlayCheckReturn_Params)
  IPC_STRUCT_TRAITS_MEMBER(status)
IPC_STRUCT_TRAITS_END()

// clang-format on

//------------------------------------------------------------------------------
// GPU Messages
// These are messages from the browser to the GPU process.

// Update the HW cursor bitmap & move to specified location.
IPC_MESSAGE_CONTROL4(OzoneGpuMsg_CursorSet,
                     gfx::AcceleratedWidget,
                     std::vector<SkBitmap>,
                     gfx::Point /* location */,
                     int /* frame_delay_ms */)

// Move the HW cursor to the specified location.
IPC_MESSAGE_CONTROL2(OzoneGpuMsg_CursorMove,
                     gfx::AcceleratedWidget, gfx::Point)

// Explicit creation of a window. We explicitly create the window such
// that any state change in the window is not lost while the surface is
// created on the GPU side.
IPC_MESSAGE_CONTROL2(OzoneGpuMsg_CreateWindow,
                     gfx::AcceleratedWidget /* widget */,
                     gfx::Rect /* initial_bounds */)

IPC_MESSAGE_CONTROL1(OzoneGpuMsg_DestroyWindow,
                     gfx::AcceleratedWidget /* widget */)

// Updates the location and size of the widget on the screen.
IPC_MESSAGE_CONTROL2(OzoneGpuMsg_WindowBoundsChanged,
                     gfx::AcceleratedWidget /* widget */,
                     gfx::Rect /* bounds */)

// Trigger a display reconfiguration. OzoneHostMsg_UpdateNativeDisplays will be
// sent as a response.
IPC_MESSAGE_CONTROL0(OzoneGpuMsg_RefreshNativeDisplays)

// Configure a display with the specified mode at the specified location.
IPC_MESSAGE_CONTROL3(OzoneGpuMsg_ConfigureNativeDisplay,
                     int64_t,  // display ID
                     ui::DisplayMode_Params,  // display mode
                     gfx::Point)  // origin for the display

IPC_MESSAGE_CONTROL1(OzoneGpuMsg_DisableNativeDisplay,
                     int64_t)  // display ID

// |device_path| is inside of the /sys filesystem, since UDL display/input
// association logic needs to access the path components.
IPC_MESSAGE_CONTROL2(OzoneGpuMsg_AddGraphicsDevice,
                     base::FilePath /* device_path */,
                     base::FileDescriptor /* device_fd */)

IPC_MESSAGE_CONTROL1(OzoneGpuMsg_RemoveGraphicsDevice,
                     base::FilePath /* device_path */)

// Take control of the display
IPC_MESSAGE_CONTROL0(OzoneGpuMsg_TakeDisplayControl)

// Let other entity control the display
IPC_MESSAGE_CONTROL0(OzoneGpuMsg_RelinquishDisplayControl)

IPC_MESSAGE_CONTROL1(OzoneGpuMsg_GetHDCPState, int64_t /* display_id */)

IPC_MESSAGE_CONTROL2(OzoneGpuMsg_SetHDCPState,
                     int64_t /* display_id */,
                     display::HDCPState /* state */)

IPC_MESSAGE_CONTROL2(OzoneGpuMsg_SetColorMatrix,
                     int64_t,             // Display ID
                     std::vector<float>)  // Color transform matrix

IPC_MESSAGE_CONTROL3(OzoneGpuMsg_SetGammaCorrection,
                     int64_t,                                  // Display ID
                     std::vector<display::GammaRampRGBEntry>,  // Degamma lut
                     std::vector<display::GammaRampRGBEntry>)  // Gamma lut

IPC_MESSAGE_CONTROL2(OzoneGpuMsg_CheckOverlayCapabilities,
                     gfx::AcceleratedWidget /* widget */,
                     std::vector<ui::OverlayCheck_Params> /* overlays */)

//------------------------------------------------------------------------------
// Browser Messages
// These messages are from the GPU to the browser process.

// Updates the list of active displays.
IPC_MESSAGE_CONTROL1(OzoneHostMsg_UpdateNativeDisplays,
                     std::vector<ui::DisplaySnapshot_Params>)

IPC_MESSAGE_CONTROL2(OzoneHostMsg_DisplayConfigured,
                     int64_t /* display_id */,
                     bool /* status */)

// Response for OzoneGpuMsg_GetHDCPState.
IPC_MESSAGE_CONTROL3(OzoneHostMsg_HDCPStateReceived,
                     int64_t /* display_id */,
                     bool /* success */,
                     display::HDCPState /* state */)

// Response for OzoneGpuMsg_SetHDCPState.
IPC_MESSAGE_CONTROL2(OzoneHostMsg_HDCPStateUpdated,
                     int64_t /* display_id */,
                     bool /* success */)

// Response to OzoneGpuMsg_TakeDisplayControl.
IPC_MESSAGE_CONTROL1(OzoneHostMsg_DisplayControlTaken, bool /* success */)

// Response to OzoneGpuMsg_RelinquishDisplayControl.
IPC_MESSAGE_CONTROL1(OzoneHostMsg_DisplayControlRelinquished,
                     bool /* success */)

// Response to OzoneGpuMsg_CheckOverlayCapabilities. Returns list of supported
// params.
IPC_MESSAGE_CONTROL3(OzoneHostMsg_OverlayCapabilitiesReceived,
                     gfx::AcceleratedWidget /* widget */,
                     std::vector<ui::OverlayCheck_Params> /* overlays */,
                     std::vector<ui::OverlayCheckReturn_Params> /* returns */)
