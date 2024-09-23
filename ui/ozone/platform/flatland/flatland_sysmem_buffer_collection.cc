// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_sysmem_buffer_collection.h"

#include <fuchsia/sysmem/cpp/fidl.h>
#include <lib/zx/eventpair.h>

#include <bit>
#include <tuple>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/koid.h"
#include "base/task/current_thread.h"
#include "build/build_config.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/ozone/platform/flatland/flatland_surface_factory.h"
#include "ui/ozone/platform/flatland/flatland_sysmem_native_pixmap.h"

namespace ui {

namespace {

size_t RoundUp(size_t value, size_t alignment) {
  return ((value + alignment - 1) / alignment) * alignment;
}

VkFormat VkFormatForBufferFormat(gfx::BufferFormat buffer_format) {
  switch (buffer_format) {
    case gfx::BufferFormat::YVU_420:
      return VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM;

    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;

    case gfx::BufferFormat::R_8:
      return VK_FORMAT_R8_UNORM;

    case gfx::BufferFormat::RG_88:
      return VK_FORMAT_R8G8_UNORM;

    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
      return VK_FORMAT_B8G8R8A8_UNORM;

    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
      return VK_FORMAT_R8G8B8A8_UNORM;

    default:
      NOTREACHED_IN_MIGRATION();
      return VK_FORMAT_UNDEFINED;
  }
}

size_t GetBytesPerPixel(gfx::BufferFormat buffer_format) {
  switch (buffer_format) {
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::R_8:
      return 1U;

    case gfx::BufferFormat::RG_88:
      return 2U;

    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
      return 4U;

    default:
      NOTREACHED_IN_MIGRATION();
      return 1;
  }
}

bool IsYuvVkFormat(VkFormat format) {
  switch (format) {
    case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
    case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
      return true;
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_UNORM:
      return false;
    default:
      NOTREACHED_IN_MIGRATION();
      return false;
  }
}

VkFormatFeatureFlags GetFormatFeatureFlagsFromUsage(VkImageUsageFlags usage) {
  VkFormatFeatureFlags result = {};
  if (usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
    result |= VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
  }
  if (usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
    result |= VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
  }
  if (usage & VK_IMAGE_USAGE_SAMPLED_BIT) {
    result |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
  }
  if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
    result |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
  }
  if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    result |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
  }
  if (usage & VK_IMAGE_USAGE_STORAGE_BIT) {
    result |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
  }
  return result;
}

VkImageFormatConstraintsInfoFUCHSIA GetDefaultImageFormatConstraintsInfo(
    const VkImageCreateInfo& create_info) {
  DCHECK(create_info.format != VK_FORMAT_UNDEFINED);
  DCHECK(create_info.usage != 0);

  static const VkSysmemColorSpaceFUCHSIA kSrgbColorSpace = {
      VK_STRUCTURE_TYPE_SYSMEM_COLOR_SPACE_FUCHSIA, nullptr,
      static_cast<uint32_t>(fuchsia::images2::ColorSpace::SRGB)};

  static const VkSysmemColorSpaceFUCHSIA kYuvDefaultColorSpaces[] = {
      {VK_STRUCTURE_TYPE_SYSMEM_COLOR_SPACE_FUCHSIA, nullptr,
       static_cast<uint32_t>(fuchsia::images2::ColorSpace::REC709)},
      {VK_STRUCTURE_TYPE_SYSMEM_COLOR_SPACE_FUCHSIA, nullptr,
       static_cast<uint32_t>(fuchsia::images2::ColorSpace::REC601_NTSC)},
      {VK_STRUCTURE_TYPE_SYSMEM_COLOR_SPACE_FUCHSIA, nullptr,
       static_cast<uint32_t>(
           fuchsia::images2::ColorSpace::REC601_NTSC_FULL_RANGE)},
      {VK_STRUCTURE_TYPE_SYSMEM_COLOR_SPACE_FUCHSIA, nullptr,
       static_cast<uint32_t>(fuchsia::images2::ColorSpace::REC601_PAL)},
      {VK_STRUCTURE_TYPE_SYSMEM_COLOR_SPACE_FUCHSIA, nullptr,
       static_cast<uint32_t>(
           fuchsia::images2::ColorSpace::REC601_PAL_FULL_RANGE)},
  };

  bool is_yuv = IsYuvVkFormat(create_info.format);

  VkImageFormatConstraintsInfoFUCHSIA format_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_CONSTRAINTS_INFO_FUCHSIA,
      .pNext = nullptr,
      .imageCreateInfo = create_info,
      .requiredFormatFeatures =
          GetFormatFeatureFlagsFromUsage(create_info.usage),
      .sysmemPixelFormat = 0u,
      .colorSpaceCount = static_cast<uint32_t>(
          is_yuv ? std::size(kYuvDefaultColorSpaces) : 1u),
      .pColorSpaces = is_yuv ? kYuvDefaultColorSpaces : &kSrgbColorSpace,
  };
  return format_info;
}

struct ImageConstraintsInfo {
  VkImageConstraintsInfoFUCHSIA image_constraints;
  VkImageFormatConstraintsInfoFUCHSIA format_constraints;

  ImageConstraintsInfo(
      const VkImageConstraintsInfoFUCHSIA& image_constraints_in,
      const VkImageFormatConstraintsInfoFUCHSIA& format_constraints_in)
      : image_constraints(image_constraints_in),
        format_constraints(format_constraints_in) {
    image_constraints.pFormatConstraints = &format_constraints;
    image_constraints.formatConstraintsCount = 1u;
  }

  ImageConstraintsInfo(ImageConstraintsInfo&& from) = delete;
  ImageConstraintsInfo(const ImageConstraintsInfo&) = delete;
  ImageConstraintsInfo& operator=(const ImageConstraintsInfo&) = delete;
};

std::unique_ptr<ImageConstraintsInfo> InitializeImageConstraintsInfo(
    const VkImageCreateInfo& vk_image_info,
    bool allow_protected_memory) {
  VkImageFormatConstraintsInfoFUCHSIA format_constraints =
      GetDefaultImageFormatConstraintsInfo(vk_image_info);
  VkImageConstraintsInfoFUCHSIA image_constraints = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CONSTRAINTS_INFO_FUCHSIA,
      .pNext = nullptr,
      .bufferCollectionConstraints =
          VkBufferCollectionConstraintsInfoFUCHSIA{
              .sType =
                  VK_STRUCTURE_TYPE_BUFFER_COLLECTION_CONSTRAINTS_INFO_FUCHSIA,
              .pNext = nullptr,
              .minBufferCount = 1u,
              // Using the default value (0) for the fields below means that
              // there is no other constraints except for the minimum buffer
              // count.
              .maxBufferCount = 0u,
              .minBufferCountForCamping = 0u,
              .minBufferCountForDedicatedSlack = 0u,
              .minBufferCountForSharedSlack = 0u,
          },
      .flags = allow_protected_memory
                   ? VK_IMAGE_CONSTRAINTS_INFO_PROTECTED_OPTIONAL_FUCHSIA
                   : 0u,
  };

  return std::make_unique<ImageConstraintsInfo>(image_constraints,
                                                format_constraints);
}

}  // namespace

// static
bool FlatlandSysmemBufferCollection::IsNativePixmapConfigSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  switch (format) {
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::RG_88:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
      break;

    default:
      return false;
  }
  switch (usage) {
    case gfx::BufferUsage::SCANOUT:
    case gfx::BufferUsage::GPU_READ:
      break;

    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
      break;

    default:
      return false;
  }
  return true;
}

FlatlandSysmemBufferCollection::FlatlandSysmemBufferCollection() = default;

bool FlatlandSysmemBufferCollection::Initialize(
    fuchsia::sysmem2::Allocator_Sync* sysmem_allocator,
    fuchsia::ui::composition::Allocator* flatland_allocator,
    FlatlandSurfaceFactory* flatland_surface_factory,
    zx::eventpair handle,
    zx::channel sysmem_token,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    VkDevice vk_device,
    size_t min_buffer_count,
    bool register_with_flatland_allocator) {
  DCHECK(IsNativePixmapConfigSupported(format, usage));
  DCHECK(!collection_);
  DCHECK(!vk_buffer_collection_);

  handle_ = std::move(handle);
  auto koid = base::GetKoid(handle_);
  if (!koid)
    return false;
  id_ = koid.value();

  // Currently all supported |usage| values require GPU access, which requires
  // a valid VkDevice.
  if (vk_device == VK_NULL_HANDLE)
    return false;

  if (size.IsEmpty()) {
    // Buffer collection that doesn't have explicit size is expected to be
    // shared with other participants, who will determine the actual image size.
    DCHECK(sysmem_token);

    // Set nominal size of 1x1, which will be used only for
    // vkSetBufferCollectionConstraintsFUCHSIA(). The actual size of the
    // allocated buffers is determined by constraints set by other sysmem
    // clients for the same collection. Size of the Vulkan image is determined
    // by the values passed to CreateVkImage().
    min_size_ = gfx::Size(1, 1);
  } else {
    min_size_ = size;
  }

  format_ = format;
  usage_ = usage;
  vk_device_ = vk_device;
  is_protected_ = false;

  fuchsia::sysmem2::BufferCollectionTokenSyncPtr collection_token;
  if (sysmem_token) {
    collection_token.Bind(std::move(sysmem_token));
  } else {
    zx_status_t status = sysmem_allocator->AllocateSharedCollection(
        std::move(fuchsia::sysmem2::AllocatorAllocateSharedCollectionRequest{}
                      .set_token_request(collection_token.NewRequest())));
    if (status != ZX_OK) {
      ZX_DLOG(ERROR, status)
          << "fuchsia.sysmem.Allocator.AllocateSharedCollection()";
      return false;
    }
  }

  return InitializeInternal(sysmem_allocator, flatland_allocator,
                            std::move(collection_token),
                            register_with_flatland_allocator, min_buffer_count);
}

void FlatlandSysmemBufferCollection::InitializeForTesting(
    zx::eventpair handle,
    gfx::BufferUsage usage) {
  handle_ = std::move(handle);
  id_ = base::GetKoid(handle_).value();

  if (usage == gfx::BufferUsage::SCANOUT) {
    // Scanout buffers need to be registered with flatland.
    fuchsia::ui::composition::BufferCollectionExportToken export_token;
    zx::eventpair::create(0, &export_token.value,
                          &flatland_import_token_.value);
  }
}

scoped_refptr<gfx::NativePixmap>
FlatlandSysmemBufferCollection::CreateNativePixmap(
    gfx::NativePixmapHandle handle,
    gfx::Size size) {
  CHECK_LT(handle.buffer_index, num_buffers());

  DCHECK_EQ(base::GetRelatedKoid(handle.buffer_collection_handle).value(), id_);
  // sysmem always fills out settings(), buffer_settings(), coherency_domain()
  handle.ram_coherency =
      buffers_info_.settings().buffer_settings().coherency_domain() ==
      fuchsia::sysmem2::CoherencyDomain::RAM;

  // `handle.planes` need to be filled in only for mappable buffers.
  if (!is_mappable())
    return new FlatlandSysmemNativePixmap(this, std::move(handle), size);

  zx::vmo main_plane_vmo;
  DCHECK(buffers_info_.buffers()[handle.buffer_index].vmo().is_valid());
  zx_status_t status =
      buffers_info_.buffers()[handle.buffer_index].vmo().duplicate(
          ZX_RIGHT_SAME_RIGHTS, &main_plane_vmo);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "zx_handle_duplicate";
    return nullptr;
  }

  const fuchsia::sysmem2::ImageFormatConstraints& format =
      buffers_info_.settings().image_format_constraints();

  // The logic should match LogicalBufferCollection::Allocate().
  size_t stride =
      RoundUp(std::max(static_cast<size_t>(format.min_bytes_per_row()),
                       size.width() * GetBytesPerPixel(format_)),
              format.bytes_per_row_divisor());
  size_t plane_offset =
      buffers_info_.buffers()[handle.buffer_index].vmo_usable_start();
  size_t plane_size = stride * size.height();
  handle.planes.emplace_back(stride, plane_offset, plane_size,
                             std::move(main_plane_vmo));

  // For YUV images add a second plane.
  if (format_ == gfx::BufferFormat::YUV_420_BIPLANAR) {
    size_t uv_plane_offset = plane_offset + plane_size;
    size_t uv_plane_size = plane_size / 2;

    zx::vmo uv_plane_vmo;
    status =
        handle.planes[0].vmo.duplicate(ZX_RIGHT_SAME_RIGHTS, &uv_plane_vmo);
    if (status != ZX_OK) {
      ZX_DLOG(ERROR, status) << "zx_handle_duplicate";
      return nullptr;
    }

    handle.planes.emplace_back(stride, uv_plane_offset, uv_plane_size,
                               std::move(uv_plane_vmo));
    DCHECK_LE(uv_plane_offset + uv_plane_size, buffer_size_);
  }

  return new FlatlandSysmemNativePixmap(this, std::move(handle), size);
}

bool FlatlandSysmemBufferCollection::CreateVkImage(
    size_t buffer_index,
    VkDevice vk_device,
    gfx::Size size,
    VkImage* vk_image,
    VkImageCreateInfo* vk_image_info,
    VkDeviceMemory* vk_device_memory,
    VkDeviceSize* mem_allocation_size) {
  DCHECK_CALLED_ON_VALID_THREAD(vulkan_thread_checker_);

  if (vk_device_ != vk_device) {
    DLOG(FATAL) << "Tried to import NativePixmap that was created for a "
                   "different VkDevice.";
    return false;
  }

  VkBufferCollectionPropertiesFUCHSIA properties = {
      VK_STRUCTURE_TYPE_BUFFER_COLLECTION_PROPERTIES_FUCHSIA};
  if (vkGetBufferCollectionPropertiesFUCHSIA(vk_device_, vk_buffer_collection_,
                                             &properties) != VK_SUCCESS) {
    DLOG(ERROR) << "vkGetBufferCollectionPropertiesFUCHSIA failed";
    return false;
  }

  InitializeImageCreateInfo(vk_image_info, size);

  VkBufferCollectionImageCreateInfoFUCHSIA image_format_fuchsia = {
      VK_STRUCTURE_TYPE_BUFFER_COLLECTION_IMAGE_CREATE_INFO_FUCHSIA,
  };
  image_format_fuchsia.collection = vk_buffer_collection_;
  image_format_fuchsia.index = buffer_index;
  vk_image_info->pNext = &image_format_fuchsia;

  if (vkCreateImage(vk_device_, vk_image_info, nullptr, vk_image) !=
      VK_SUCCESS) {
    DLOG(ERROR) << "Failed to create VkImage.";
    return false;
  }

  vk_image_info->pNext = nullptr;

  VkMemoryRequirements requirements;
  vkGetImageMemoryRequirements(vk_device, *vk_image, &requirements);

  uint32_t viable_memory_types =
      properties.memoryTypeBits & requirements.memoryTypeBits;
  uint32_t memory_type = std::countr_zero(viable_memory_types);

  VkMemoryDedicatedAllocateInfoKHR dedicated_allocate = {
      VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR};
  dedicated_allocate.image = *vk_image;
  VkImportMemoryBufferCollectionFUCHSIA buffer_collection_info = {
      VK_STRUCTURE_TYPE_IMPORT_MEMORY_BUFFER_COLLECTION_FUCHSIA,
      &dedicated_allocate};
  buffer_collection_info.collection = vk_buffer_collection_;
  buffer_collection_info.index = buffer_index;

  VkMemoryAllocateInfo alloc_info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                     &buffer_collection_info};
  alloc_info.allocationSize = requirements.size;
  alloc_info.memoryTypeIndex = memory_type;

  if (vkAllocateMemory(vk_device_, &alloc_info, nullptr, vk_device_memory) !=
      VK_SUCCESS) {
    DLOG(ERROR) << "Failed to create VkMemory from sysmem buffer.";
    vkDestroyImage(vk_device_, *vk_image, nullptr);
    *vk_image = VK_NULL_HANDLE;
    return false;
  }

  if (vkBindImageMemory(vk_device_, *vk_image, *vk_device_memory, 0u) !=
      VK_SUCCESS) {
    DLOG(ERROR) << "Failed to bind sysmem buffer to a VkImage.";
    vkDestroyImage(vk_device_, *vk_image, nullptr);
    *vk_image = VK_NULL_HANDLE;
    vkFreeMemory(vk_device_, *vk_device_memory, nullptr);
    *vk_device_memory = VK_NULL_HANDLE;
    return false;
  }

  *mem_allocation_size = requirements.size;

  return true;
}

fuchsia::ui::composition::BufferCollectionImportToken
FlatlandSysmemBufferCollection::GetFlatlandImportToken() const {
  DCHECK(HasFlatlandImportToken());

  fuchsia::ui::composition::BufferCollectionImportToken import_dup;
  zx_status_t status = flatland_import_token_.value.duplicate(
      ZX_RIGHT_SAME_RIGHTS, &import_dup.value);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "BufferCollectionImportToken duplicate failed";
  }
  return import_dup;
}

bool FlatlandSysmemBufferCollection::HasFlatlandImportToken() const {
  return flatland_import_token_.value.is_valid();
}

void FlatlandSysmemBufferCollection::AddOnReleasedCallback(
    base::OnceClosure on_released) {
  on_released_.push_back(std::move(on_released));
}

FlatlandSysmemBufferCollection::~FlatlandSysmemBufferCollection() {
  if (vk_buffer_collection_ != VK_NULL_HANDLE) {
    vkDestroyBufferCollectionFUCHSIA(vk_device_, vk_buffer_collection_,
                                     nullptr);
  }

  if (collection_)
    collection_->Release();
}

bool FlatlandSysmemBufferCollection::InitializeInternal(
    fuchsia::sysmem2::Allocator_Sync* sysmem_allocator,
    fuchsia::ui::composition::Allocator* flatland_allocator,
    fuchsia::sysmem2::BufferCollectionTokenSyncPtr collection_token,
    bool register_with_flatland_allocator,
    size_t min_buffer_count) {
  fidl::InterfaceHandle<fuchsia::sysmem2::BufferCollectionToken>
      collection_token_for_vulkan;
  collection_token->Duplicate(std::move(
      fuchsia::sysmem2::BufferCollectionTokenDuplicateRequest{}
          .set_rights_attenuation_mask(ZX_RIGHT_SAME_RIGHTS)
          .set_token_request(collection_token_for_vulkan.NewRequest())));

  fidl::InterfaceHandle<fuchsia::sysmem2::BufferCollectionToken>
      collection_token_for_flatland;
  if (register_with_flatland_allocator) {
    collection_token->Duplicate(std::move(
        fuchsia::sysmem2::BufferCollectionTokenDuplicateRequest{}
            .set_rights_attenuation_mask(ZX_RIGHT_SAME_RIGHTS)
            .set_token_request(collection_token_for_flatland.NewRequest())));
  }

  fuchsia::sysmem2::Node_Sync_Result sync_result;
  zx_status_t status = collection_token->Sync(&sync_result);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "fuchsia.sysmem.BufferCollectionToken.Sync()";
    return false;
  }

  status = sysmem_allocator->BindSharedCollection(
      std::move(fuchsia::sysmem2::AllocatorBindSharedCollectionRequest{}
                    .set_token(std::move(collection_token))
                    .set_buffer_collection_request(collection_.NewRequest())));
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "fuchsia.sysmem.Allocator.BindSharedCollection()";
    return false;
  }

  // Set |min_buffer_count| constraints.
  fuchsia::sysmem2::BufferCollectionConstraints constraints;
  if (is_mappable()) {
    constraints.mutable_usage()->set_cpu(fuchsia::sysmem2::CPU_USAGE_READ |
                                         fuchsia::sysmem2::CPU_USAGE_WRITE);

    auto& memory_constraints = *constraints.mutable_buffer_memory_constraints();
    memory_constraints.set_ram_domain_supported(true);
    memory_constraints.set_cpu_domain_supported(true);
  } else {
    constraints.mutable_usage()->set_none(fuchsia::sysmem2::NONE_USAGE);
  }

  constraints.set_min_buffer_count(min_buffer_count);

  status = collection_->SetConstraints(std::move(
      fuchsia::sysmem2::BufferCollectionSetConstraintsRequest{}.set_constraints(
          std::move(constraints))));
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status)
        << "fuchsia.sysmem.BufferCollection.SetConstraints()";
    return false;
  }

  // Set Flatland allocator constraints.
  if (register_with_flatland_allocator) {
    DCHECK(flatland_allocator);
    fuchsia::ui::composition::BufferCollectionExportToken export_token;
    status = zx::eventpair::create(0, &export_token.value,
                                   &flatland_import_token_.value);

    fuchsia::ui::composition::RegisterBufferCollectionArgs args;
    args.set_export_token(std::move(export_token));
    args.set_buffer_collection_token(
        fuchsia::sysmem::BufferCollectionTokenHandle(
            collection_token_for_flatland.TakeChannel()));
    args.set_usage(
        fuchsia::ui::composition::RegisterBufferCollectionUsage::DEFAULT);
    flatland_allocator->RegisterBufferCollection(
        std::move(args),
        [](fuchsia::ui::composition::Allocator_RegisterBufferCollection_Result
               result) {
          if (result.is_err()) {
            LOG(FATAL) << "RegisterBufferCollection failed";
          }
        });
  }

  // Set Vulkan constraints.
  zx::channel token_channel = collection_token_for_vulkan.TakeChannel();
  VkBufferCollectionCreateInfoFUCHSIA buffer_collection_create_info = {
      VK_STRUCTURE_TYPE_BUFFER_COLLECTION_CREATE_INFO_FUCHSIA};
  buffer_collection_create_info.collectionToken = token_channel.get();
  if (vkCreateBufferCollectionFUCHSIA(vk_device_,
                                      &buffer_collection_create_info, nullptr,
                                      &vk_buffer_collection_) != VK_SUCCESS) {
    vk_buffer_collection_ = VK_NULL_HANDLE;
    DLOG(ERROR) << "vkCreateBufferCollectionFUCHSIA() failed";
    return false;
  }

  // vkCreateBufferCollectionFUCHSIA() takes ownership of the token on success.
  std::ignore = token_channel.release();

  VkImageCreateInfo image_create_info;
  InitializeImageCreateInfo(&image_create_info, min_size_);

  // TODO(crbug.com/42050415): Instead of always allowing protected memory,
  // Chrome should query if the Vulkan physical device supports protected
  // memory and only set the flag if it is supported.
  auto image_constraints_info = InitializeImageConstraintsInfo(
      image_create_info, /* allow_protected_memory */ true);

  auto result = vkSetBufferCollectionImageConstraintsFUCHSIA(
      vk_device_, vk_buffer_collection_,
      &image_constraints_info->image_constraints);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vkSetBufferCollectionConstraintsFUCHSIA() failed:"
                << result;
    return false;
  }

  fuchsia::sysmem2::BufferCollection_WaitForAllBuffersAllocated_Result
      wait_result;
  status = collection_->WaitForAllBuffersAllocated(&wait_result);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "fuchsia.sysmem.BufferCollection failed";
    return false;
  }
  if (!wait_result.is_response()) {
    if (wait_result.is_framework_err()) {
      LOG(ERROR) << "fuchsia.sysmem.BufferCollection::WaitForBuffersAllocated()"
          " failed (framework_err): "
          << fidl::ToUnderlying(wait_result.framework_err());
    } else {
      LOG(ERROR) << "fuchsia.sysmem.BufferCollection::WaitForBuffersAllocated()"
          " failed (err): " << static_cast<uint32_t>(wait_result.err());
    }
    return false;
  }
  buffers_info_ =
      std::move(*wait_result.response().mutable_buffer_collection_info());

  // sysmem always fills out buffers(), settings(), image_format_constraints(),
  // size_bytes(), is_secure()
  DCHECK_GE(buffers_info_.buffers().size(), min_buffer_count);
  DCHECK(buffers_info_.settings().has_image_format_constraints());

  buffer_size_ = buffers_info_.settings().buffer_settings().size_bytes();
  is_protected_ = buffers_info_.settings().buffer_settings().is_secure();

  handle_watch_ =
      std::make_unique<base::MessagePumpForIO::ZxHandleWatchController>(
          FROM_HERE);
  bool watch_result = base::CurrentIOThread::Get()->WatchZxHandle(
      handle_.get(), /*persistent=*/false, ZX_EVENTPAIR_PEER_CLOSED,
      handle_watch_.get(), this);

  if (!watch_result) {
    DLOG(ERROR) << "Failed to add a watcher for sysmem buffer token";
    return false;
  }

  // CreateVkImage() should always be called on the same thread, but it may be
  // different from the thread that called Initialize().
  DETACH_FROM_THREAD(vulkan_thread_checker_);

  return true;
}

void FlatlandSysmemBufferCollection::InitializeImageCreateInfo(
    VkImageCreateInfo* vk_image_info,
    gfx::Size size) {
  *vk_image_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  vk_image_info->flags = is_protected_ ? VK_IMAGE_CREATE_PROTECTED_BIT : 0u;
  vk_image_info->imageType = VK_IMAGE_TYPE_2D;
  vk_image_info->format = VkFormatForBufferFormat(format_);
  vk_image_info->extent = VkExtent3D{static_cast<uint32_t>(size.width()),
                                     static_cast<uint32_t>(size.height()), 1};
  vk_image_info->mipLevels = 1;
  vk_image_info->arrayLayers = 1;
  vk_image_info->samples = VK_SAMPLE_COUNT_1_BIT;
  vk_image_info->tiling =
      is_mappable() ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;

  vk_image_info->usage = VK_IMAGE_USAGE_SAMPLED_BIT |
                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  if (usage_ == gfx::BufferUsage::SCANOUT) {
    vk_image_info->usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }

  vk_image_info->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  vk_image_info->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

void FlatlandSysmemBufferCollection::OnZxHandleSignalled(zx_handle_t handle,
                                                         zx_signals_t signals) {
  DCHECK_EQ(handle, handle_.get());
  DCHECK_EQ(signals, ZX_EVENTPAIR_PEER_CLOSED);

  // Keep a reference to `this` to ensure it's not destroyed while calling the
  // callbacks.
  scoped_refptr<FlatlandSysmemBufferCollection> self(this);

  for (auto& callback : on_released_) {
    std::move(callback).Run();
  }
  on_released_.clear();
}

}  // namespace ui
