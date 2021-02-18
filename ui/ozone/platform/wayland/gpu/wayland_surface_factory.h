// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_SURFACE_FACTORY_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_SURFACE_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "ui/gl/gl_surface.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/public/surface_factory_ozone.h"

namespace ui {

class WaylandConnection;
class WaylandBufferManagerGpu;

class WaylandSurfaceFactory : public SurfaceFactoryOzone {
 public:
  WaylandSurfaceFactory(WaylandConnection* connection,
                        WaylandBufferManagerGpu* buffer_manager);
  ~WaylandSurfaceFactory() override;

  // SurfaceFactoryOzone overrides:
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
  scoped_refptr<gfx::NativePixmap> CreateNativePixmapFromHandle(
      gfx::AcceleratedWidget widget,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::NativePixmapHandle handle) override;

 private:
  WaylandConnection* const connection_;
  WaylandBufferManagerGpu* const buffer_manager_;
  std::unique_ptr<GLOzone> egl_implementation_;

  DISALLOW_COPY_AND_ASSIGN(WaylandSurfaceFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_SURFACE_FACTORY_H_
