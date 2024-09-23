// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/vulkan_implementation_flatland.h"

#include <lib/zx/channel.h>
#include <vulkan/vulkan.h>

#include <memory>
#include <tuple>

#include "base/files/file_path.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/functional/callback_helpers.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "gpu/vulkan/fuchsia/vulkan_fuchsia_ext.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "gpu/vulkan/vulkan_util.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/ozone/platform/flatland/flatland_surface.h"
#include "ui/ozone/platform/flatland/flatland_surface_factory.h"
#include "ui/ozone/platform/flatland/flatland_sysmem_buffer_collection.h"
#include "ui/ozone/platform/flatland/flatland_window.h"
#include "ui/ozone/platform/flatland/flatland_window_manager.h"

namespace ui {

VulkanImplementationFlatland::VulkanImplementationFlatland(
    FlatlandSurfaceFactory* flatland_surface_factory,
    FlatlandSysmemBufferManager* flatland_sysmem_buffer_manager,
    bool use_swiftshader,
    bool allow_protected_memory)
    : VulkanImplementation(use_swiftshader, allow_protected_memory),
      flatland_sysmem_buffer_manager_(flatland_sysmem_buffer_manager) {}

VulkanImplementationFlatland::~VulkanImplementationFlatland() = default;

bool VulkanImplementationFlatland::InitializeVulkanInstance(
    bool using_surface) {
  DCHECK(!using_surface);

  base::FilePath path(use_swiftshader() ? "libvk_swiftshader.so"
                                        : "libvulkan.so");
  std::vector<const char*> required_extensions;
  std::vector<const char*> required_layers;
  return vulkan_instance_.Initialize(path, required_extensions,
                                     required_layers);
}

gpu::VulkanInstance* VulkanImplementationFlatland::GetVulkanInstance() {
  return &vulkan_instance_;
}

std::unique_ptr<gpu::VulkanSurface>
VulkanImplementationFlatland::CreateViewSurface(gfx::AcceleratedWidget window) {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

bool VulkanImplementationFlatland::GetPhysicalDevicePresentationSupport(
    VkPhysicalDevice physical_device,
    const std::vector<VkQueueFamilyProperties>& queue_family_properties,
    uint32_t queue_family_index) {
  return true;
}

std::vector<const char*>
VulkanImplementationFlatland::GetRequiredDeviceExtensions() {
  std::vector<const char*> result = {
      VK_FUCHSIA_EXTERNAL_MEMORY_EXTENSION_NAME,
      VK_FUCHSIA_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
      VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
      VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME,
      VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
      VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
      VK_KHR_MAINTENANCE1_EXTENSION_NAME,
  };

  // Following extensions are not supported by Swiftshader.
  if (!use_swiftshader()) {
    result.push_back(VK_FUCHSIA_BUFFER_COLLECTION_EXTENSION_NAME);
    result.push_back(VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME);
  }

  return result;
}

std::vector<const char*>
VulkanImplementationFlatland::GetOptionalDeviceExtensions() {
  return {};
}

VkFence VulkanImplementationFlatland::CreateVkFenceForGpuFence(
    VkDevice vk_device) {
  NOTIMPLEMENTED();
  return VK_NULL_HANDLE;
}

std::unique_ptr<gfx::GpuFence>
VulkanImplementationFlatland::ExportVkFenceToGpuFence(VkDevice vk_device,
                                                      VkFence vk_fence) {
  NOTIMPLEMENTED();
  return nullptr;
}

VkExternalSemaphoreHandleTypeFlagBits
VulkanImplementationFlatland::GetExternalSemaphoreHandleType() {
  return VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_ZIRCON_EVENT_BIT_FUCHSIA;
}

bool VulkanImplementationFlatland::CanImportGpuMemoryBuffer(
    gpu::VulkanDeviceQueue* device_queue,
    gfx::GpuMemoryBufferType memory_buffer_type) {
  return memory_buffer_type == gfx::NATIVE_PIXMAP;
}

std::unique_ptr<gpu::VulkanImage>
VulkanImplementationFlatland::CreateImageFromGpuMemoryHandle(
    gpu::VulkanDeviceQueue* device_queue,
    gfx::GpuMemoryBufferHandle gmb_handle,
    gfx::Size size,
    VkFormat vk_format,
    const gfx::ColorSpace& color_space) {
  if (gmb_handle.type != gfx::NATIVE_PIXMAP)
    return nullptr;

  if (!gmb_handle.native_pixmap_handle.buffer_collection_handle) {
    DLOG(ERROR) << "NativePixmapHandle.buffer_collection_handle is not set.";
    return nullptr;
  }

  auto collection = flatland_sysmem_buffer_manager_->GetCollectionByHandle(
      gmb_handle.native_pixmap_handle.buffer_collection_handle);
  if (!collection) {
    DLOG(ERROR) << "Tried to use an unknown buffer collection ID.";
    return nullptr;
  }
  VkImage vk_image = VK_NULL_HANDLE;
  VkImageCreateInfo vk_image_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  VkDeviceMemory vk_device_memory = VK_NULL_HANDLE;
  VkDeviceSize vk_device_size = 0;
  if (!collection->CreateVkImage(gmb_handle.native_pixmap_handle.buffer_index,
                                 device_queue->GetVulkanDevice(), size,
                                 &vk_image, &vk_image_info, &vk_device_memory,
                                 &vk_device_size)) {
    DLOG(ERROR) << "CreateVkImage failed.";
    return nullptr;
  }

  std::optional<gpu::VulkanYCbCrInfo> ycbcr_info;
  if (collection->format() == gfx::BufferFormat::YUV_420_BIPLANAR) {
    VkSamplerYcbcrModelConversion ycbcr_conversion =
        (color_space.GetMatrixID() == gfx::ColorSpace::MatrixID::BT709)
            ? VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709
            : VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601;

    // Currently sysmem doesn't specify location of chroma samples relative to
    // luma (see fxbug.dev/13677). Assume they are cosited with luma. Y'CbCr
    // info here must match the values passed for the same buffer in
    // FuchsiaVideoDecoder. |format_features| are resolved later in the GPU
    // process before the ycbcr info is passed to Skia.
    ycbcr_info = gpu::VulkanYCbCrInfo(
        vk_image_info.format, /*external_format=*/0, ycbcr_conversion,
        VK_SAMPLER_YCBCR_RANGE_ITU_NARROW, VK_CHROMA_LOCATION_COSITED_EVEN,
        VK_CHROMA_LOCATION_COSITED_EVEN, /*format_features=*/0);
  }

  auto image = gpu::VulkanImage::Create(
      device_queue, vk_image, vk_device_memory, size, vk_image_info.format,
      vk_image_info.tiling, vk_device_size, 0 /* memory_type_index */,
      ycbcr_info, vk_image_info.usage, vk_image_info.flags);

  if (image->format() != vk_format) {
    DLOG(ERROR) << "Unexpected format " << vk_format << " vs "
                << image->format();
    image->Destroy();
    return nullptr;
  }

  image->set_queue_family_index(VK_QUEUE_FAMILY_EXTERNAL);
  image->set_native_pixmap(collection->CreateNativePixmap(
      std::move(gmb_handle.native_pixmap_handle), size));
  return image;
}

void VulkanImplementationFlatland::RegisterSysmemBufferCollection(
    VkDevice device,
    zx::eventpair service_handle,
    zx::channel sysmem_token,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    gfx::Size size,
    size_t min_buffer_count,
    bool register_with_flatland_allocator) {
  flatland_sysmem_buffer_manager_->ImportSysmemBufferCollection(
      device, std::move(service_handle), std::move(sysmem_token), size, format,
      usage, min_buffer_count, register_with_flatland_allocator);
}

}  // namespace ui
