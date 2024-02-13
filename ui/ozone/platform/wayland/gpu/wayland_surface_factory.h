// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_SURFACE_FACTORY_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/gl/gl_surface.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/public/drm_modifiers_filter.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

class WaylandConnection;
class WaylandBufferManagerGpu;

class WaylandSurfaceFactory : public SurfaceFactoryOzone {
 public:
  WaylandSurfaceFactory(WaylandConnection* connection,
                        WaylandBufferManagerGpu* buffer_manager);

  WaylandSurfaceFactory(const WaylandSurfaceFactory&) = delete;
  WaylandSurfaceFactory& operator=(const WaylandSurfaceFactory&) = delete;

  ~WaylandSurfaceFactory() override;

  // SurfaceFactoryOzone overrides:
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
  std::optional<gfx::BufferFormat> GetPreferredFormatForSolidColor()
      const override;
  bool SupportsDrmModifiersFilter() const override;
  void SetDrmModifiersFilter(
      std::unique_ptr<DrmModifiersFilter> filter) override;

  bool SupportsNativePixmaps() const;

  std::vector<gfx::BufferFormat> GetSupportedFormatsForTexturing()
      const override;

 private:
  const raw_ptr<WaylandConnection, AcrossTasksDanglingUntriaged> connection_;
  const raw_ptr<WaylandBufferManagerGpu, AcrossTasksDanglingUntriaged>
      buffer_manager_;
  std::unique_ptr<GLOzone> egl_implementation_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_SURFACE_FACTORY_H_
