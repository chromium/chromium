// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/vulkan_implementation_wayland.h"

#include <vulkan/vulkan_wayland.h>

#include "base/base_paths.h"
#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "gpu/vulkan/vulkan_util.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace ui {

namespace {

bool InitializeVulkanFunctionPointers(
    const base::FilePath& path,
    gpu::VulkanFunctionPointers* vulkan_function_pointers) {
  base::NativeLibraryLoadError native_library_load_error;
  vulkan_function_pointers->vulkan_loader_library =
      base::LoadNativeLibrary(path, &native_library_load_error);
  return !!vulkan_function_pointers->vulkan_loader_library;
}

}  // namespace

VulkanImplementationWayland::VulkanImplementationWayland(bool use_swiftshader)
    : gpu::VulkanImplementation(use_swiftshader) {}

VulkanImplementationWayland::~VulkanImplementationWayland() {}

bool VulkanImplementationWayland::InitializeVulkanInstance(bool using_surface) {
  std::vector<const char*> required_extensions = {
      VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_CAPABILITIES_EXTENSION_NAME,
  };

  auto* vulkan_function_pointers = gpu::GetVulkanFunctionPointers();

  base::FilePath path;
  if (use_swiftshader()) {
    if (!base::PathService::Get(base::DIR_MODULE, &path))
      return false;

    path = path.Append("libvk_swiftshader.so");
  } else {
    path = base::FilePath("libvulkan.so.1");
  }

  if (!InitializeVulkanFunctionPointers(path, vulkan_function_pointers))
    return false;

  return vulkan_instance_.Initialize(required_extensions, {});
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
  NOTREACHED();
  return VK_NULL_HANDLE;
}

std::unique_ptr<gfx::GpuFence>
VulkanImplementationWayland::ExportVkFenceToGpuFence(VkDevice vk_device,
                                                     VkFence vk_fence) {
  NOTREACHED();
  return nullptr;
}

VkSemaphore VulkanImplementationWayland::CreateExternalSemaphore(
    VkDevice vk_device) {
  return gpu::CreateExternalVkSemaphore(
      vk_device, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT);
}

VkSemaphore VulkanImplementationWayland::ImportSemaphoreHandle(
    VkDevice vk_device,
    gpu::SemaphoreHandle sync_handle) {
  return gpu::ImportVkSemaphoreHandle(vk_device, std::move(sync_handle));
}

gpu::SemaphoreHandle VulkanImplementationWayland::GetSemaphoreHandle(
    VkDevice vk_device,
    VkSemaphore vk_semaphore) {
  return gpu::GetVkSemaphoreHandle(
      vk_device, vk_semaphore, VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT);
}

VkExternalMemoryHandleTypeFlagBits
VulkanImplementationWayland::GetExternalImageHandleType() {
  return VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
}

bool VulkanImplementationWayland::CanImportGpuMemoryBuffer(
    gfx::GpuMemoryBufferType memory_buffer_type) {
  NOTIMPLEMENTED();
  return false;
}

std::unique_ptr<gpu::VulkanImage>
VulkanImplementationWayland::CreateImageFromGpuMemoryHandle(
    gpu::VulkanDeviceQueue* device_queue,
    gfx::GpuMemoryBufferHandle gmb_handle,
    gfx::Size size,
    VkFormat vk_formate) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace ui
