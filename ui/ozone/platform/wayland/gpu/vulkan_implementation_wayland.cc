// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/vulkan_implementation_wayland.h"

#include <vulkan/vulkan_wayland.h>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "gpu/vulkan/vulkan_util.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace ui {

VulkanImplementationWayland::VulkanImplementationWayland(bool use_swiftshader)
    : gpu::VulkanImplementation(use_swiftshader) {}

VulkanImplementationWayland::~VulkanImplementationWayland() {}

bool VulkanImplementationWayland::InitializeVulkanInstance(bool using_surface) {
  std::vector<const char*> required_extensions = {
      VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
  };

  base::FilePath path;
  if (use_swiftshader()) {
    if (!base::PathService::Get(base::DIR_MODULE, &path))
      return false;

    path = path.Append("libvk_swiftshader.so");
  } else {
    path = base::FilePath("libvulkan.so.1");
  }

  return vulkan_instance_.Initialize(path, required_extensions, {});
}

gpu::VulkanInstance* VulkanImplementationWayland::GetVulkanInstance() {
  return &vulkan_instance_;
}

std::unique_ptr<gpu::VulkanSurface>
VulkanImplementationWayland::CreateViewSurface(gfx::AcceleratedWidget window) {
  NOTIMPLEMENTED();
  return nullptr;
}

bool VulkanImplementationWayland::GetPhysicalDevicePresentationSupport(
    VkPhysicalDevice device,
    const std::vector<VkQueueFamilyProperties>& queue_family_properties,
    uint32_t queue_family_index) {
  NOTIMPLEMENTED();
  return false;
}

std::vector<const char*>
VulkanImplementationWayland::GetRequiredDeviceExtensions() {
  std::vector<const char*> result{
      VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
      VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME,
  };

  if (!use_swiftshader())
    result.push_back(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME);
  return result;
}

std::vector<const char*>
VulkanImplementationWayland::GetOptionalDeviceExtensions() {
  // VK_EXT_image_drm_format_modifier is not well supported right now, so has to
  // request as an optional extension.
  return {
      VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
  };
}

VkFence VulkanImplementationWayland::CreateVkFenceForGpuFence(
    VkDevice vk_device) {
  NOTREACHED_IN_MIGRATION();
  return VK_NULL_HANDLE;
}

std::unique_ptr<gfx::GpuFence>
VulkanImplementationWayland::ExportVkFenceToGpuFence(VkDevice vk_device,
                                                     VkFence vk_fence) {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

VkExternalSemaphoreHandleTypeFlagBits
VulkanImplementationWayland::GetExternalSemaphoreHandleType() {
  return VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
}

bool VulkanImplementationWayland::CanImportGpuMemoryBuffer(
    gpu::VulkanDeviceQueue* device_queue,
    gfx::GpuMemoryBufferType memory_buffer_type) {
  const auto& enabled_extensions = device_queue->enabled_extensions();
  return gfx::HasExtension(enabled_extensions,
                           VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME) &&
         gfx::HasExtension(enabled_extensions,
                           VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME) &&
         memory_buffer_type == gfx::GpuMemoryBufferType::NATIVE_PIXMAP;
}

std::unique_ptr<gpu::VulkanImage>
VulkanImplementationWayland::CreateImageFromGpuMemoryHandle(
    gpu::VulkanDeviceQueue* device_queue,
    gfx::GpuMemoryBufferHandle gmb_handle,
    gfx::Size size,
    VkFormat vk_format,
    const gfx::ColorSpace& color_space) {
  constexpr auto kUsage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  auto tiling = gmb_handle.native_pixmap_handle.modifier ==
                        gfx::NativePixmapHandle::kNoModifier
                    ? VK_IMAGE_TILING_OPTIMAL
                    : VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
  return gpu::VulkanImage::CreateFromGpuMemoryBufferHandle(
      device_queue, std::move(gmb_handle), size, vk_format, kUsage, /*flags=*/0,
      tiling, VK_QUEUE_FAMILY_FOREIGN_EXT);
}

}  // namespace ui
