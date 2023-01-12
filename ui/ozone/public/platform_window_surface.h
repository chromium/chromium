// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_PLATFORM_WINDOW_SURFACE_H_
#define UI_OZONE_PUBLIC_PLATFORM_WINDOW_SURFACE_H_

#include "base/component_export.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_FUCHSIA)
#include <fuchsia/images/cpp/fidl.h>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/public/overlay_plane.h"

#endif  // BUILDFLAG(IS_FUCHSIA)

namespace ui {

// Rendering and presentation API agnostic platform surface object.
//
// This object should be created prior to creation of a GLSurface,
// VulkanSurface, or software surface that presents to a PlatformWindow.
//
// It is basically the Viz service version of PlatformWindow, and is intended to
// contain the windowing system connection for a particular window's rendering
// surface.
//
// However, currently it is only used by SkiaRenderer on Fuchsia and does
// nothing in all other cases.
//
// TODO(spang): If we go this way, we should be consistent. You should have to
// have a PlatformWindowSurface before building a GLSurface or software surface
// as well.
class COMPONENT_EXPORT(OZONE_BASE) PlatformWindowSurface {
 public:
  virtual ~PlatformWindowSurface() = default;

#if BUILDFLAG(IS_FUCHSIA)
  using BufferPresentedCallback =
      base::OnceCallback<void(const gfx::PresentationFeedback& feedback)>;
  using SwapCompletionCallback =
      base::OnceCallback<void(gfx::SwapCompletionResult)>;
  // Schedules the primary and optional overlay planes for presentation.
  virtual void Present(scoped_refptr<gfx::NativePixmap> primary_plane_pixmap,
                       std::vector<ui::OverlayPlane> overlays,
                       std::vector<gfx::GpuFenceHandle> acquire_fences,
                       std::vector<gfx::GpuFenceHandle> release_fences,
                       SwapCompletionCallback completion_callback,
                       BufferPresentedCallback presentation_callback) {}
#endif  // BUILDFLAG(IS_FUCHSIA)

  // Note: GL surface may be created through the GLOzone interface.
  // However, you must still create a PlatformWindowSurface and keep it alive in
  // order to present.
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_PLATFORM_WINDOW_SURFACE_H_
