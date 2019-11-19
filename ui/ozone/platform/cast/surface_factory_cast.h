// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_CAST_SURFACE_FACTORY_CAST_H_
#define UI_OZONE_PLATFORM_CAST_SURFACE_FACTORY_CAST_H_

#include <memory>
#include <vector>

#include "base/macros.h"
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
  ~SurfaceFactoryCast() override;

  // SurfaceFactoryOzone implementation:
  std::vector<gl::GLImplementation> GetAllowedGLImplementations() override;
  GLOzone* GetGLOzone(gl::GLImplementation implementation) override;
  std::unique_ptr<SurfaceOzoneCanvas> CreateCanvasForWidget(
      gfx::AcceleratedWidget widget,
      base::TaskRunner* task_runner) override;
  scoped_refptr<gfx::NativePixmap> CreateNativePixmap(
      gfx::AcceleratedWidget widget,
      VkDevice vk_device,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override;

 private:
  std::unique_ptr<GLOzoneEglCast> egl_implementation_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceFactoryCast);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_CAST_SURFACE_FACTORY_CAST_H_
