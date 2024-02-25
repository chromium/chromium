// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_HEADLESS_HEADLESS_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_HEADLESS_HEADLESS_SURFACE_FACTORY_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "ui/ozone/public/gl_ozone.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

class HeadlessSurfaceFactory : public SurfaceFactoryOzone {
 public:
  explicit HeadlessSurfaceFactory(base::FilePath base_path);

  HeadlessSurfaceFactory(const HeadlessSurfaceFactory&) = delete;
  HeadlessSurfaceFactory& operator=(const HeadlessSurfaceFactory&) = delete;

  ~HeadlessSurfaceFactory() override;

  // SurfaceFactoryOzone:
  std::vector<gl::GLImplementationParts> GetAllowedGLImplementations() override;
  GLOzone* GetGLOzone(const gl::GLImplementationParts& implementation) override;
#if BUILDFLAG(ENABLE_VULKAN)
  std::unique_ptr<gpu::VulkanImplementation> CreateVulkanImplementation(
      bool use_swiftshader,
      bool allow_protected_memory) override;
#endif  // BUILDFLAG(ENABLE_VULKAN)
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
  void CheckBasePath() const;

  // Base path for window output PNGs.
  base::FilePath base_path_;

  std::unique_ptr<GLOzone> swiftshader_implementation_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_HEADLESS_HEADLESS_SURFACE_FACTORY_H_
