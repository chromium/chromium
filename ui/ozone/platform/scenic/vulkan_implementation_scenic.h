// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_VULKAN_IMPLEMENTATION_SCENIC_H_
#define UI_OZONE_PLATFORM_SCENIC_VULKAN_IMPLEMENTATION_SCENIC_H_

#include <fuchsia/ui/scenic/cpp/fidl.h>
#include <memory>

#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "ui/ozone/public/mojom/scenic_gpu_host.mojom.h"

namespace ui {

class ScenicSurfaceFactory;
class SysmemBufferManager;

class VulkanImplementationScenic : public gpu::VulkanImplementation {
 public:
  VulkanImplementationScenic(ScenicSurfaceFactory* scenic_surface_factory,
                             SysmemBufferManager* sysmem_buffer_manager,
                             bool allow_protected_memory,
                             bool enforce_protected_memory);
  ~VulkanImplementationScenic() override;

  // VulkanImplementation:
  bool InitializeVulkanInstance(bool using_surface) override;
  gpu::VulkanInstance* GetVulkanInstance() override;
  std::unique_ptr<gpu::VulkanSurface> CreateViewSurface(
      gfx::AcceleratedWidget window) override;
  bool GetPhysicalDevicePresentationSupport(
      VkPhysicalDevice device,
      const std::vector<VkQueueFamilyProperties>& queue_family_properties,
      uint32_t queue_family_index) override;
  std::vector<const char*> GetRequiredDeviceExtensions() override;
  VkFence CreateVkFenceForGpuFence(VkDevice vk_device) override;
  std::unique_ptr<gfx::GpuFence> ExportVkFenceToGpuFence(
      VkDevice vk_device,
      VkFence vk_fence) override;
  VkSemaphore CreateExternalSemaphore(VkDevice vk_device) override;
  VkSemaphore ImportSemaphoreHandle(VkDevice vk_device,
                                    gpu::SemaphoreHandle handle) override;
  gpu::SemaphoreHandle GetSemaphoreHandle(VkDevice vk_device,
                                          VkSemaphore vk_semaphore) override;
  VkExternalMemoryHandleTypeFlagBits GetExternalImageHandleType() override;
  bool CanImportGpuMemoryBuffer(
      gfx::GpuMemoryBufferType memory_buffer_type) override;
  bool CreateImageFromGpuMemoryHandle(
      VkDevice vk_device,
      gfx::GpuMemoryBufferHandle gmb_handle,
      gfx::Size size,
      VkImage* vk_image,
      VkImageCreateInfo* vk_image_info,
      VkDeviceMemory* vk_device_memory,
      VkDeviceSize* mem_allocation_size,
      base::Optional<gpu::VulkanYCbCrInfo>* ycbcr_info) override;
  std::unique_ptr<gpu::SysmemBufferCollection> RegisterSysmemBufferCollection(
      VkDevice device,
      gfx::SysmemBufferCollectionId id,
      zx::channel token) override;

 private:
  ScenicSurfaceFactory* const scenic_surface_factory_;
  SysmemBufferManager* const sysmem_buffer_manager_;

  gpu::VulkanInstance vulkan_instance_;

  DISALLOW_COPY_AND_ASSIGN(VulkanImplementationScenic);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_VULKAN_IMPLEMENTATION_SCENIC_H_
