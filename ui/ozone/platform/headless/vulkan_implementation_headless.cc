// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/headless/vulkan_implementation_headless.h"

#include <optional>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/scoped_environment_variable_override.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "gpu/vulkan/vulkan_util.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/ozone/platform/headless/vulkan_surface_headless.h"

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/zx/channel.h>
#endif

namespace ui {

VulkanImplementationHeadless::VulkanImplementationHeadless(bool use_swiftshader)
    : gpu::VulkanImplementation(use_swiftshader) {}

bool VulkanImplementationHeadless::InitializeVulkanInstance(
    bool using_surface) {
  using_surface_ = using_surface;

  std::vector<const char*> required_extensions = {
      VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME};
  if (using_surface_) {
    required_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    required_extensions.push_back(VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME);
  }

  base::FilePath path;
#if BUILDFLAG(IS_FUCHSIA)
  path = base::FilePath(use_swiftshader() ? "libvk_swiftshader.so"
                                          : "libvulkan.so");
#else
  if (use_swiftshader()) {
    if (!base::PathService::Get(base::DIR_MODULE, &path))
      return false;
    path = path.Append("libvk_swiftshader.so");
  } else {
    path = base::FilePath("libvulkan.so.1");
  }
#endif

  return vulkan_instance_.Initialize(path, required_extensions, {});
}

gpu::VulkanInstance* VulkanImplementationHeadless::GetVulkanInstance() {
  return &vulkan_instance_;
}

std::unique_ptr<gpu::VulkanSurface>
VulkanImplementationHeadless::CreateViewSurface(gfx::AcceleratedWidget window) {
  if (!using_surface_)
    return nullptr;
  return VulkanSurfaceHeadless::Create(vulkan_instance_.vk_instance(), window);
}

bool VulkanImplementationHeadless::GetPhysicalDevicePresentationSupport(
    VkPhysicalDevice device,
    const std::vector<VkQueueFamilyProperties>& queue_family_properties,
    uint32_t queue_family_index) {
  // TODO(samans): Don't early out once Swiftshader supports this method.
  // https://crbug.com/swiftshader/129
  if (use_swiftshader())
    return true;
  // Should this be false?
  return true;
}

std::vector<const char*>
VulkanImplementationHeadless::GetRequiredDeviceExtensions() {
  std::vector<const char*> extensions;
  if (using_surface_)
    extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  return extensions;
}

std::vector<const char*>
VulkanImplementationHeadless::GetOptionalDeviceExtensions() {
  return {
      VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
      VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
      VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME,
      VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
      VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
  };
}

VkFence VulkanImplementationHeadless::CreateVkFenceForGpuFence(
    VkDevice vk_device) {
  NOTREACHED_IN_MIGRATION();
  return VK_NULL_HANDLE;
}

std::unique_ptr<gfx::GpuFence>
VulkanImplementationHeadless::ExportVkFenceToGpuFence(VkDevice vk_device,
                                                      VkFence vk_fence) {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

VkExternalSemaphoreHandleTypeFlagBits
VulkanImplementationHeadless::GetExternalSemaphoreHandleType() {
#if BUILDFLAG(IS_LINUX)
  return VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
#else
  return VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
#endif
}

bool VulkanImplementationHeadless::CanImportGpuMemoryBuffer(
    gpu::VulkanDeviceQueue* device_queue,
    gfx::GpuMemoryBufferType memory_buffer_type) {
#if BUILDFLAG(IS_LINUX)
  const auto& enabled_extensions = device_queue->enabled_extensions();
  return gfx::HasExtension(enabled_extensions,
                           VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME) &&
         gfx::HasExtension(enabled_extensions,
                           VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME) &&
         memory_buffer_type == gfx::GpuMemoryBufferType::NATIVE_PIXMAP;
#else
  return memory_buffer_type == gfx::GpuMemoryBufferType::NATIVE_PIXMAP;
#endif
}

std::unique_ptr<gpu::VulkanImage>
VulkanImplementationHeadless::CreateImageFromGpuMemoryHandle(
    gpu::VulkanDeviceQueue* device_queue,
    gfx::GpuMemoryBufferHandle gmb_handle,
    gfx::Size size,
    VkFormat vk_format,
    const gfx::ColorSpace& color_space) {
  constexpr auto kUsage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  auto tiling = VK_IMAGE_TILING_OPTIMAL;
#if BUILDFLAG(IS_LINUX)
  if (gmb_handle.native_pixmap_handle.modifier !=
      gfx::NativePixmapHandle::kNoModifier) {
    tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
  }
#endif  // BUILDFLAG(IS_LINUX)
  return gpu::VulkanImage::CreateFromGpuMemoryBufferHandle(
      device_queue, std::move(gmb_handle), size, vk_format, kUsage, /*flags=*/0,
      tiling, VK_QUEUE_FAMILY_EXTERNAL);
}

#if BUILDFLAG(IS_FUCHSIA)
void VulkanImplementationHeadless::RegisterSysmemBufferCollection(
    VkDevice device,
    zx::eventpair service_handle,
    zx::channel sysmem_token,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gfx::Size size,
    size_t min_buffer_count,
    bool register_with_image_pipe) {
  NOTIMPLEMENTED();
}
#endif  // BUILDFLAG(IS_FUCHSIA)

}  // namespace ui
