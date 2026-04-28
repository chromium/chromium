// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_CA_LAYER_PARAMS_H_
#define UI_GFX_CA_LAYER_PARAMS_H_

#include "base/component_export.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/size.h"

#if BUILDFLAG(IS_APPLE)
#include "ui/gfx/mac/io_surface.h"
#endif

namespace gfx {

// The parameters required to add a composited frame to a CALayer. This
// is used only on macOS.
struct COMPONENT_EXPORT(GFX) CALayerParams {
  CALayerParams();
  CALayerParams(CALayerParams&& params);
  CALayerParams(const CALayerParams& params) = delete;
  CALayerParams& operator=(CALayerParams&& params);
  CALayerParams& operator=(const CALayerParams& params) = delete;
  ~CALayerParams();

  // Return a clone of `this`. To avoid accidental stalls, the resulting clone
  // will not retain `ca_context_fence_mach_port`.
  CALayerParams CloneWithoutFence() const;

  // If `this` or `other` has a valid `ca_context_fence_mach_port` then equality
  // will return false.
  bool operator==(const CALayerParams& other) const;

  // Helper to short-circuit code to handle CALayerParams on non-macOS
  // platforms.
  bool IsEmpty() const;

  // Can be used to instantiate a CALayerTreeHost in the browser process, which
  // will display a CALayerTree rooted in the GPU process. This is non-zero when
  // using remote CoreAnimation.
  uint32_t ca_context_id = 0;

#if BUILDFLAG(IS_APPLE)
  // A port that, until it is deleted, will keep the previous CAContext alive
  // and frozen until it is freed.
  base::apple::ScopedMachSendRight ca_context_fence_mach_port;

  // Used to set the contents of a CALayer in the browser to an IOSurface that
  // is specified by the GPU process. This is non-null iff |ca_context_id| is
  // zero.
  gfx::ScopedRefCountedIOSurfaceMachPort io_surface_mach_port;
#endif

  // The geometry of the frame.
  gfx::Size pixel_size;
  float scale_factor = 1.f;
};

}  // namespace gfx

#endif  // UI_GFX_CA_LAYER_PARAMS_H_
