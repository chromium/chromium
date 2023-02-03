// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_CA_LAYER_PARAMS_H_
#define UI_GFX_CA_LAYER_PARAMS_H_

#include "build/build_config.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gfx_export.h"

#if BUILDFLAG(IS_APPLE)
#include "ui/gfx/mac/io_surface.h"
#endif

namespace gfx {

// The parameters required to add a composited frame to a CALayer. This
// is used only on macOS.
struct GFX_EXPORT CALayerParams {
  CALayerParams();
  CALayerParams(CALayerParams&& params);
  CALayerParams(const CALayerParams& params);
  CALayerParams& operator=(CALayerParams&& params);
  CALayerParams& operator=(const CALayerParams& params);
  ~CALayerParams();

  bool operator==(const CALayerParams& params) const {
    return is_empty == params.is_empty &&
           ca_context_id == params.ca_context_id &&
#if BUILDFLAG(IS_APPLE)
           io_surface_mach_port == params.io_surface_mach_port &&
#endif
           pixel_size == params.pixel_size &&
           scale_factor == params.scale_factor;
  }

  // The |is_empty| flag is used to short-circuit code to handle CALayerParams
  // on non-macOS platforms.
  bool is_empty = true;

  // Can be used to instantiate a CALayerTreeHost in the browser process, which
  // will display a CALayerTree rooted in the GPU process. This is non-zero when
  // using remote CoreAnimation.
  uint32_t ca_context_id = 0;

  // Used to set the contents of a CALayer in the browser to an IOSurface that
  // is specified by the GPU process. This is non-null iff |ca_context_id| is
  // zero.
#if BUILDFLAG(IS_APPLE)
  gfx::ScopedRefCountedIOSurfaceMachPort io_surface_mach_port;
#endif

  // The geometry of the frame.
  gfx::Size pixel_size;
  float scale_factor = 1.f;
};

}  // namespace gfx

#endif  // UI_GFX_CA_LAYER_PARAMS_H_
