// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_PLATFORM_WINDOW_SURFACE_H_
#define UI_OZONE_PUBLIC_PLATFORM_WINDOW_SURFACE_H_

#include "base/component_export.h"

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

  // Note: GL & Vulkan surface are created through the GLOzone &
  // VulkanImplementation interfaces, respectively.
  //
  // However, you must still create a PlatformWindowSurface and keep it alive in
  // order to present.
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_PLATFORM_WINDOW_SURFACE_H_
