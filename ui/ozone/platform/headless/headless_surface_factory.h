// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_HEADLESS_HEADLESS_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_HEADLESS_HEADLESS_SURFACE_FACTORY_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "ui/ozone/public/gl_ozone.h"
#include "ui/ozone/public/surface_factory_ozone.h"

#if BUILDFLAG(IS_LINUX)
#include "base/files/scoped_file.h"
#include "ui/gl/gl_display.h"
#endif

namespace ui {

class GbmDevice;

class HeadlessSurfaceFactory : public SurfaceFactoryOzone {
 public:
  explicit HeadlessSurfaceFactory(base::FilePath base_path);

  HeadlessSurfaceFactory(const HeadlessSurfaceFactory&) = delete;
  HeadlessSurfaceFactory& operator=(const HeadlessSurfaceFactory&) = delete;

  ~HeadlessSurfaceFactory() override;

  bool SupportsNativePixmaps() const;
#if BUILDFLAG(IS_LINUX)
  void InitializeGbm();
  gl::EGLDisplayPlatform GetNativeDisplay() const;
  bool IsNativePixmapConfigSupported(viz::SharedImageFormat format,
                                     gfx::BufferUsage usage) const;
#endif

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
      viz::SharedImageFormat format,
      gfx::BufferUsage usage,
      std::optional<gfx::Size> framebuffer_size) override;
#if BUILDFLAG(IS_LINUX)
  bool CanCreateNativePixmapForFormat(viz::SharedImageFormat format) override;
  scoped_refptr<gfx::NativePixmap> CreateNativePixmapFromHandle(
      gfx::AcceleratedWidget widget,
      gfx::Size size,
      viz::SharedImageFormat format,
      gfx::NativePixmapHandle handle) override;
#endif

 private:
  void CheckBasePath() const;

  // Base path for window output PNGs.
  base::FilePath base_path_;

  std::unique_ptr<GLOzone> swiftshader_implementation_;

#if BUILDFLAG(IS_LINUX)
  bool gbm_init_attempted_ = false;
  // gbm_create_device borrows the fd, so it must outlive |gbm_device_|.
  base::ScopedFD drm_fd_;
  std::unique_ptr<GbmDevice> gbm_device_;
  gl::EGLDisplayPlatform native_display_;
#endif
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_HEADLESS_HEADLESS_SURFACE_FACTORY_H_
