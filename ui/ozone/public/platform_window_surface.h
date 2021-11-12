// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_PLATFORM_WINDOW_SURFACE_H_
#define UI_OZONE_PUBLIC_PLATFORM_WINDOW_SURFACE_H_

#include "base/component_export.h"
#include "build/build_config.h"

#if defined(OS_FUCHSIA)
#include <fuchsia/images/cpp/fidl.h>
#endif  // defined(OS_FUCHSIA)

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
  virtual ~PlatformWindowSurface() {}

#if defined(OS_FUCHSIA)
  // Sets the texture of the surface to a new image pipe.
  virtual bool SetTextureToNewImagePipe(
      fidl::InterfaceRequest<fuchsia::images::ImagePipe2>
          image_pipe_request) = 0;

  // Updates overlays layout for the current frame.
  virtual void FlushOverlaysLayout(
      const std::vector<zx::event>& acquire_fences) {}
#endif  // defined(OS_FUCHSIA)

  // Note: GL surface may be created through the GLOzone interface.
  // However, you must still create a PlatformWindowSurface and keep it alive in
  // order to present.
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_PLATFORM_WINDOW_SURFACE_H_
