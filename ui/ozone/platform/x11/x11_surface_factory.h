// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_X11_X11_SURFACE_FACTORY_H_

#include <memory>
#include <vector>

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

  X11SurfaceFactory(const X11SurfaceFactory&) = delete;
  X11SurfaceFactory& operator=(const X11SurfaceFactory&) = delete;

  ~X11SurfaceFactory() override;

  // SurfaceFactoryOzone:
  std::vector<gl::GLImplementationParts> GetAllowedGLImplementations() override;
  GLOzone* GetGLOzone(const gl::GLImplementationParts& implementation) override;
#if BUILDFLAG(ENABLE_VULKAN)
  std::unique_ptr<gpu::VulkanImplementation> CreateVulkanImplementation(
      bool use_swiftshader,
      bool allow_protected_memory) override;
#endif
  std::unique_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget widget) override;
  scoped_refptr<gfx::NativePixmap> CreateNativePixmap(
      gfx::AcceleratedWidget widget,
      gpu::VulkanDeviceQueue* device_queue,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      std::optional<gfx::Size> framebuffer_size = std::nullopt) override;
  bool CanCreateNativePixmapForFormat(gfx::BufferFormat format) override;
  void CreateNativePixmapAsync(gfx::AcceleratedWidget widget,
                               gpu::VulkanDeviceQueue* device_queue,
                               gfx::Size size,
                               gfx::BufferFormat format,
                               gfx::BufferUsage usage,
                               NativePixmapCallback callback) override;
  scoped_refptr<gfx::NativePixmap> CreateNativePixmapFromHandle(
      gfx::AcceleratedWidget widget,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::NativePixmapHandle handle) override;

  std::vector<gfx::BufferFormat> GetSupportedFormatsForTexturing()
      const override;

 private:
  std::unique_ptr<GLOzone> egl_implementation_;

  std::unique_ptr<x11::Connection> connection_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_SURFACE_FACTORY_H_
