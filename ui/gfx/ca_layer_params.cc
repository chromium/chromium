// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ca_layer_params.h"

namespace gfx {

CALayerParams::CALayerParams() = default;
CALayerParams::~CALayerParams() = default;
CALayerParams::CALayerParams(CALayerParams&& params) = default;
CALayerParams& CALayerParams::operator=(CALayerParams&& params) = default;

CALayerParams CALayerParams::CloneWithoutFence() const {
  CALayerParams result;
  result.ca_context_id = ca_context_id;
#if BUILDFLAG(IS_APPLE)
  result.io_surface_mach_port = io_surface_mach_port;
#endif
  result.pixel_size = pixel_size;
  result.scale_factor = scale_factor;
  return result;
}

bool CALayerParams::operator==(const CALayerParams& other) const {
  return ca_context_id == other.ca_context_id &&
#if BUILDFLAG(IS_APPLE)
         !ca_context_fence_mach_port.is_valid() &&
         !other.ca_context_fence_mach_port.is_valid() &&
         io_surface_mach_port == other.io_surface_mach_port &&
#endif
         pixel_size == other.pixel_size && scale_factor == other.scale_factor;
}

bool CALayerParams::IsEmpty() const {
#if BUILDFLAG(IS_APPLE)
  return !ca_context_id && !io_surface_mach_port;
#else
  return ca_context_id == 0;
#endif
}

}  // namespace gfx
