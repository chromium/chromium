// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_FLATLAND_VULKAN_IMPLEMENTATION_FLATLAND_H_
#define UI_OZONE_PLATFORM_FLATLAND_VULKAN_IMPLEMENTATION_FLATLAND_H_

#include <memory>

#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_instance.h"

namespace ui {

class FlatlandSurfaceFactory;
class FlatlandSysmemBufferManager;

class VulkanImplementationFlatland : public gpu::VulkanImplementation {
 public:
  VulkanImplementationFlatland(
      FlatlandSurfaceFactory* flatland_surface_factory,
      FlatlandSysmemBufferManager* flatland_sysmem_buffer_manager,
      bool use_swiftshader,
      bool allow_protected_memory);
  ~VulkanImplementationFlatland() override;
  VulkanImplementationFlatland(const VulkanImplementationFlatland&) = delete;
  VulkanImplementationFlatland& operator=(const VulkanImplementationFlatland&) =
      delete;

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
  std::vector<const char*> GetOptionalDeviceExtensions() override;
  VkFence CreateVkFenceForGpuFence(VkDevice vk_device) override;
  std::unique_ptr<gfx::GpuFence> ExportVkFenceToGpuFence(
      VkDevice vk_device,
      VkFence vk_fence) override;
  VkExternalSemaphoreHandleTypeFlagBits GetExternalSemaphoreHandleType()
      override;
  bool CanImportGpuMemoryBuffer(
      gpu::VulkanDeviceQueue* device_queue,
      gfx::GpuMemoryBufferType memory_buffer_type) override;
  std::unique_ptr<gpu::VulkanImage> CreateImageFromGpuMemoryHandle(
      gpu::VulkanDeviceQueue* device_queue,
      gfx::GpuMemoryBufferHandle gmb_handle,
      gfx::Size size,
      VkFormat vk_format,
      const gfx::ColorSpace& color_space) override;
  void RegisterSysmemBufferCollection(
      VkDevice device,
      zx::eventpair service_handle,
      zx::channel sysmem_token,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      gfx::Size size,
      size_t min_buffer_count,
      bool register_with_flatland_allocator) override;

 private:
  FlatlandSysmemBufferManager* const flatland_sysmem_buffer_manager_;

  gpu::VulkanInstance vulkan_instance_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_FLATLAND_VULKAN_IMPLEMENTATION_FLATLAND_H_
