// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/vulkan_implementation_gbm.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "gpu/vulkan/vulkan_util.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace ui {

VulkanImplementationGbm::VulkanImplementationGbm(bool allow_protected_memory)
    : VulkanImplementation(/*use_switfshader=*/false, allow_protected_memory) {}

VulkanImplementationGbm::~VulkanImplementationGbm() = default;

bool VulkanImplementationGbm::InitializeVulkanInstance(bool using_surface) {
  DLOG_IF(ERROR, using_surface) << "VK_KHR_surface is not supported.";

  std::vector<const char*> required_extensions = {
      "VK_KHR_get_physical_device_properties2",
  };
  if (!vulkan_instance_.Initialize(base::FilePath("libvulkan.so.1"),
                                   required_extensions, {})) {
    return false;
  }
  vkGetPhysicalDeviceExternalFenceProperties_ =
      reinterpret_cast<PFN_vkGetPhysicalDeviceExternalFenceProperties>(
          vkGetInstanceProcAddr(vulkan_instance_.vk_instance(),
                                "vkGetPhysicalDeviceExternalFenceProperties"));
  if (!vkGetPhysicalDeviceExternalFenceProperties_) {
    return false;
  }

  vkGetFenceFdKHR_ = reinterpret_cast<PFN_vkGetFenceFdKHR>(
      vkGetInstanceProcAddr(vulkan_instance_.vk_instance(), "vkGetFenceFdKHR"));
  if (!vkGetFenceFdKHR_) {
    return false;
  }

  return true;
}

gpu::VulkanInstance* VulkanImplementationGbm::GetVulkanInstance() {
  return &vulkan_instance_;
}

std::unique_ptr<gpu::VulkanSurface> VulkanImplementationGbm::CreateViewSurface(
    gfx::AcceleratedWidget window) {
  return nullptr;
}

bool VulkanImplementationGbm::GetPhysicalDevicePresentationSupport(
    VkPhysicalDevice physical_device,
    const std::vector<VkQueueFamilyProperties>& queue_family_properties,
    uint32_t queue_family_index) {
  VkPhysicalDeviceExternalFenceInfo external_fence_info = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_FENCE_INFO,
      .handleType = VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT};
  VkExternalFenceProperties external_fence_properties;
  vkGetPhysicalDeviceExternalFenceProperties_(
      physical_device, &external_fence_info, &external_fence_properties);
  if (!(external_fence_properties.externalFenceFeatures &
        VK_EXTERNAL_FENCE_FEATURE_EXPORTABLE_BIT)) {
    return false;
  }

  return true;
}

std::vector<const char*>
VulkanImplementationGbm::GetRequiredDeviceExtensions() {
  return {VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME,
          VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME,
          VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
          VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
          VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
          VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME};
}

std::vector<const char*>
VulkanImplementationGbm::GetOptionalDeviceExtensions() {
  return {
      VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
      VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
  };
}

VkFence VulkanImplementationGbm::CreateVkFenceForGpuFence(VkDevice vk_device) {
  VkFenceCreateInfo fence_create_info = {};
  fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  VkExportFenceCreateInfo fence_export_create_info = {};
  fence_create_info.pNext = &fence_export_create_info;
  fence_export_create_info.sType = VK_STRUCTURE_TYPE_EXPORT_FENCE_CREATE_INFO;
  fence_export_create_info.handleTypes =
      VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT;

  VkFence fence;
  VkResult result =
      vkCreateFence(vk_device, &fence_create_info, nullptr, &fence);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vkCreateFence failed: " << result;
    return VK_NULL_HANDLE;
  }

  return fence;
}

std::unique_ptr<gfx::GpuFence> VulkanImplementationGbm::ExportVkFenceToGpuFence(
    VkDevice vk_device,
    VkFence vk_fence) {
  VkFenceGetFdInfoKHR fence_get_fd_info = {};
  fence_get_fd_info.sType = VK_STRUCTURE_TYPE_FENCE_GET_FD_INFO_KHR;
  fence_get_fd_info.fence = vk_fence;
  fence_get_fd_info.handleType = VK_EXTERNAL_FENCE_HANDLE_TYPE_SYNC_FD_BIT;
  int fence_fd = -1;
  VkResult result = vkGetFenceFdKHR_(vk_device, &fence_get_fd_info, &fence_fd);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vkGetFenceFdKHR failed: " << result;
    return nullptr;
  }

  gfx::GpuFenceHandle gpu_fence_handle;
  gpu_fence_handle.Adopt(base::ScopedFD(fence_fd));
  return std::make_unique<gfx::GpuFence>(std::move(gpu_fence_handle));
}

VkExternalSemaphoreHandleTypeFlagBits
VulkanImplementationGbm::GetExternalSemaphoreHandleType() {
  return VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT;
}

bool VulkanImplementationGbm::CanImportGpuMemoryBuffer(
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
VulkanImplementationGbm::CreateImageFromGpuMemoryHandle(
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
      device_queue, std::move(gmb_handle), size, vk_format, kUsage,
      allow_protected_memory() ? VK_IMAGE_CREATE_PROTECTED_BIT : 0, tiling,
      VK_QUEUE_FAMILY_FOREIGN_EXT);
}

}  // namespace ui
