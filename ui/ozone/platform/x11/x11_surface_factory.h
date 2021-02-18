// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_X11_X11_SURFACE_FACTORY_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "gpu/vulkan/buildflags.h"
#include "ui/gfx/x/connection.h"
#include "ui/gl/gl_surface.h"
#include "ui/ozone/public/gl_ozone.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

// Handles GL initialization and surface/context creation for X11.
class X11SurfaceFactory : public SurfaceFactoryOzone {
 public:
  explicit X11SurfaceFactory(std::unique_ptr<x11::Connection> connection);
  ~X11SurfaceFactory() override;

  // SurfaceFactoryOzone:
  std::vector<gl::GLImplementation> GetAllowedGLImplementations() override;
  GLOzone* GetGLOzone(gl::GLImplementation implementation) override;
#if BUILDFLAG(ENABLE_VULKAN)
  std::unique_ptr<gpu::VulkanImplementation> CreateVulkanImplementation(
      bool use_swiftshader,
      bool allow_protected_memory,
      bool enforce_protected_memory) override;
#endif
  std::unique_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget widget) override;
  scoped_refptr<gfx::NativePixmap> CreateNativePixmap(
      gfx::AcceleratedWidget widget,
      VkDevice vk_device,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      base::Optional<gfx::Size> framebuffer_size = base::nullopt) override;
  void CreateNativePixmapAsync(gfx::AcceleratedWidget widget,
                               VkDevice vk_device,
                               gfx::Size size,
                               gfx::BufferFormat format,
                               gfx::BufferUsage usage,
                               NativePixmapCallback callback) override;

 private:
  std::unique_ptr<GLOzone> glx_implementation_;
  std::unique_ptr<GLOzone> egl_implementation_;

  std::unique_ptr<x11::Connection> connection_;

  DISALLOW_COPY_AND_ASSIGN(X11SurfaceFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_SURFACE_FACTORY_H_
