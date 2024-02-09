// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_CAST_SURFACE_FACTORY_CAST_H_
#define UI_OZONE_PLATFORM_CAST_SURFACE_FACTORY_CAST_H_

#include <memory>
#include <vector>

#include "ui/gfx/geometry/size.h"
#include "ui/ozone/platform/cast/gl_ozone_egl_cast.h"
#include "ui/ozone/public/gl_ozone.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace chromecast {
class CastEglPlatform;
}

namespace ui {

// SurfaceFactoryOzone implementation for OzonePlatformCast.
class SurfaceFactoryCast : public SurfaceFactoryOzone {
 public:
  SurfaceFactoryCast();
  explicit SurfaceFactoryCast(
      std::unique_ptr<chromecast::CastEglPlatform> egl_platform);

  SurfaceFactoryCast(const SurfaceFactoryCast&) = delete;
  SurfaceFactoryCast& operator=(const SurfaceFactoryCast&) = delete;

  ~SurfaceFactoryCast() override;

  // SurfaceFactoryOzone implementation:
  std::vector<gl::GLImplementationParts> GetAllowedGLImplementations() override;
  GLOzone* GetGLOzone(const gl::GLImplementationParts& implementation) override;
  std::unique_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget widget) override;
  scoped_refptr<gfx::NativePixmap> CreateNativePixmap(
      gfx::AcceleratedWidget widget,
      gpu::VulkanDeviceQueue* device_queue,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      std::optional<gfx::Size> framebuffer_size = std::nullopt) override;

 private:
  std::unique_ptr<GLOzoneEglCast> egl_implementation_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_CAST_SURFACE_FACTORY_CAST_H_
