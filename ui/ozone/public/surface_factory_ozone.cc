// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/surface_factory_ozone.h"

#include <stdlib.h>
#include <memory>

#include "base/command_line.h"
#include "gpu/vulkan/buildflags.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gl/gl_implementation.h"
#include "ui/ozone/public/overlay_surface.h"
#include "ui/ozone/public/platform_window_surface.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

#if BUILDFLAG(ENABLE_VULKAN)
#include "gpu/vulkan/vulkan_instance.h"
#endif

namespace ui {

SurfaceFactoryOzone::SurfaceFactoryOzone() {}

SurfaceFactoryOzone::~SurfaceFactoryOzone() {}

std::vector<gl::GLImplementationParts>
SurfaceFactoryOzone::GetAllowedGLImplementations() {
  return std::vector<gl::GLImplementationParts>();
}

GLOzone* SurfaceFactoryOzone::GetGLOzone(
    const gl::GLImplementationParts& implementation) {
  return nullptr;
}

GLOzone* SurfaceFactoryOzone::GetCurrentGLOzone() {
  return GetGLOzone(gl::GetGLImplementationParts());
}

#if BUILDFLAG(ENABLE_VULKAN)
std::unique_ptr<gpu::VulkanImplementation>
SurfaceFactoryOzone::CreateVulkanImplementation(bool use_swiftshader,
                                                bool allow_protected_memory) {
  return nullptr;
}

scoped_refptr<gfx::NativePixmap>
SurfaceFactoryOzone::CreateNativePixmapForVulkan(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    VkDevice vk_device,
    VkDeviceMemory* vk_device_memory,
    VkImage* vk_image) {
  NOTIMPLEMENTED();
  return nullptr;
}
#endif

std::unique_ptr<PlatformWindowSurface>
SurfaceFactoryOzone::CreatePlatformWindowSurface(
    gfx::AcceleratedWidget widget) {
  return nullptr;
}

std::unique_ptr<OverlaySurface> SurfaceFactoryOzone::CreateOverlaySurface(
    gfx::AcceleratedWidget widget) {
  return nullptr;
}

std::unique_ptr<SurfaceOzoneCanvas> SurfaceFactoryOzone::CreateCanvasForWidget(
    gfx::AcceleratedWidget widget) {
  return nullptr;
}

scoped_refptr<gfx::NativePixmap> SurfaceFactoryOzone::CreateNativePixmap(
    gfx::AcceleratedWidget widget,
    gpu::VulkanDeviceQueue* device_queue,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    std::optional<gfx::Size> framebuffer_size) {
  return nullptr;
}

bool SurfaceFactoryOzone::CanCreateNativePixmapForFormat(
    gfx::BufferFormat format) {
  // It's up to specific implementations of this method to report an inability
  // to create native pixmap handles for a specific format.
  return true;
}

void SurfaceFactoryOzone::CreateNativePixmapAsync(
    gfx::AcceleratedWidget widget,
    gpu::VulkanDeviceQueue* device_queue,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    NativePixmapCallback callback) {
  std::move(callback).Run(nullptr);
}

scoped_refptr<gfx::NativePixmap>
SurfaceFactoryOzone::CreateNativePixmapFromHandle(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::NativePixmapHandle handle) {
  return nullptr;
}

scoped_refptr<gfx::NativePixmap>
SurfaceFactoryOzone::CreateNativePixmapForProtectedBufferHandle(
    gfx::AcceleratedWidget widget,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::NativePixmapHandle handle) {
  return nullptr;
}

void SurfaceFactoryOzone::SetGetProtectedNativePixmapDelegate(
    const GetProtectedNativePixmapCallback&
        get_protected_native_pixmap_callback) {}

bool SurfaceFactoryOzone::SupportsDrmModifiersFilter() const {
  return false;
}

void SurfaceFactoryOzone::SetDrmModifiersFilter(
    std::unique_ptr<DrmModifiersFilter> filter) {
  NOTIMPLEMENTED();
}

std::vector<gfx::BufferFormat>
SurfaceFactoryOzone::GetSupportedFormatsForTexturing() const {
  return std::vector<gfx::BufferFormat>();
}

std::vector<gfx::BufferFormat>
SurfaceFactoryOzone::GetSupportedFormatsForGLNativePixmapImport() {
  std::vector<gfx::BufferFormat> supported_buffer_formats;
  auto* gl_ozone = GetCurrentGLOzone();
  if (!gl_ozone) {
    return supported_buffer_formats;
  }

  for (int j = 0; j <= static_cast<int>(gfx::BufferFormat::LAST); ++j) {
    const gfx::BufferFormat buffer_format = static_cast<gfx::BufferFormat>(j);
    if (gl_ozone->CanImportNativePixmap(buffer_format)) {
      supported_buffer_formats.push_back(buffer_format);
    }
  }
  return supported_buffer_formats;
}

std::optional<gfx::BufferFormat>
SurfaceFactoryOzone::GetPreferredFormatForSolidColor() const {
  return std::nullopt;
}

}  // namespace ui
