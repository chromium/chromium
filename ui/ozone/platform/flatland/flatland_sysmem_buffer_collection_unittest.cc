// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_sysmem_buffer_collection.h"

#include <fuchsia/sysmem2/cpp/fidl.h>
#include <lib/zx/channel.h>
#include <lib/zx/eventpair.h>
#include <lib/zx/vmo.h>

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_log.h"
#include "base/test/task_environment.h"
#include "components/viz/common/resources/shared_image_format.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/ozone/public/native_pixmap_usage.h"

using ::testing::_;
using ::testing::HasSubstr;

namespace ui {

namespace {

constexpr uint32_t kAllocatedWidth = 64;
constexpr uint32_t kAllocatedHeight = 64;
constexpr uint32_t kBytesPerPixel = 4;
constexpr size_t kAllocatedBufferBytes =
    kAllocatedWidth * kAllocatedHeight * kBytesPerPixel;

// Global variables for mock Vulkan API control.
VkDeviceSize g_mock_vk_image_requirements_size = 0;
uint32_t g_mock_vk_image_requirements_memory_type_bits = 0xFFFFFFFF;

VkResult MockVkCreateImage(VkDevice device,
                           const VkImageCreateInfo* pCreateInfo,
                           const VkAllocationCallbacks* pAllocator,
                           VkImage* pImage) {
  *pImage = reinterpret_cast<VkImage>(0x12345678);
  return VK_SUCCESS;
}

void MockVkGetImageMemoryRequirements(
    VkDevice device,
    VkImage image,
    VkMemoryRequirements* pMemoryRequirements) {
  pMemoryRequirements->size = g_mock_vk_image_requirements_size;
  pMemoryRequirements->alignment = 64;
  pMemoryRequirements->memoryTypeBits =
      g_mock_vk_image_requirements_memory_type_bits;
}

void MockVkDestroyImage(VkDevice device,
                        VkImage image,
                        const VkAllocationCallbacks* pAllocator) {}

VkResult MockVkAllocateMemory(VkDevice device,
                              const VkMemoryAllocateInfo* pAllocateInfo,
                              const VkAllocationCallbacks* pAllocator,
                              VkDeviceMemory* pMemory) {
  *pMemory = reinterpret_cast<VkDeviceMemory>(0x87654321);
  return VK_SUCCESS;
}

VkResult MockVkBindImageMemory(VkDevice device,
                               VkImage image,
                               VkDeviceMemory memory,
                               VkDeviceSize memoryOffset) {
  return VK_SUCCESS;
}

void MockVkFreeMemory(VkDevice device,
                      VkDeviceMemory memory,
                      const VkAllocationCallbacks* pAllocator) {}

VkResult MockVkGetBufferCollectionPropertiesFUCHSIA(
    VkDevice device,
    VkBufferCollectionFUCHSIA collection,
    VkBufferCollectionPropertiesFUCHSIA* pProperties) {
  pProperties->memoryTypeBits = 0xFFFFFFFF;
  return VK_SUCCESS;
}

fuchsia::sysmem2::BufferCollectionInfo MakeBufferCollectionInfo(
    bool include_vmo) {
  fuchsia::sysmem2::BufferCollectionInfo info;

  auto& settings = *info.mutable_settings();
  auto& buffer_settings = *settings.mutable_buffer_settings();
  buffer_settings.set_size_bytes(kAllocatedBufferBytes);
  buffer_settings.set_coherency_domain(fuchsia::sysmem2::CoherencyDomain::CPU);

  auto& image_constraints = *settings.mutable_image_format_constraints();
  image_constraints.set_min_size(fuchsia::math::SizeU{1, 1});
  image_constraints.set_max_size(
      fuchsia::math::SizeU{kAllocatedWidth, kAllocatedHeight});
  image_constraints.set_min_bytes_per_row(kAllocatedWidth * kBytesPerPixel);
  image_constraints.set_bytes_per_row_divisor(1);

  auto& buffer = info.mutable_buffers()->emplace_back();
  buffer.set_vmo_usable_start(0);
  if (include_vmo) {
    zx::vmo vmo;
    zx_status_t status = zx::vmo::create(kAllocatedBufferBytes, 0, &vmo);
    CHECK_EQ(status, ZX_OK);
    buffer.set_vmo(std::move(vmo));
  }

  return info;
}

scoped_refptr<FlatlandSysmemBufferCollection> MakeCollection(
    NativePixmapUsageSet usage,
    zx::eventpair* out_client_handle,
    bool include_vmo,
    VkDevice vk_device = VK_NULL_HANDLE) {
  zx::eventpair service_handle;
  zx::eventpair::create(0, &service_handle, out_client_handle);

  auto collection = base::MakeRefCounted<FlatlandSysmemBufferCollection>();
  collection->InitializeForTesting(
      std::move(service_handle), usage, viz::SinglePlaneFormat::kRGBA_8888,
      MakeBufferCollectionInfo(include_vmo), vk_device);
  return collection;
}

gfx::NativePixmapHandle MakePixmapHandle(const zx::eventpair& client_handle) {
  gfx::NativePixmapHandle handle;
  handle.buffer_index = 0;
  zx_status_t status = client_handle.duplicate(
      ZX_RIGHT_SAME_RIGHTS, &handle.buffer_collection_handle);
  CHECK_EQ(status, ZX_OK);
  return handle;
}

}  // namespace

class FlatlandSysmemBufferCollectionTest : public ::testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  void SetUp() override {
    // Save original Vulkan function pointers.
    orig_create_image_ = gpu::GetVulkanFunctionPointers()->vkCreateImage.get();
    orig_get_reqs_ =
        gpu::GetVulkanFunctionPointers()->vkGetImageMemoryRequirements.get();
    orig_destroy_image_ =
        gpu::GetVulkanFunctionPointers()->vkDestroyImage.get();
    orig_alloc_mem_ = gpu::GetVulkanFunctionPointers()->vkAllocateMemory.get();
    orig_bind_mem_ = gpu::GetVulkanFunctionPointers()->vkBindImageMemory.get();
    orig_free_mem_ = gpu::GetVulkanFunctionPointers()->vkFreeMemory.get();
    orig_get_props_ = gpu::GetVulkanFunctionPointers()
                          ->vkGetBufferCollectionPropertiesFUCHSIA.get();

    // Set mock Vulkan function pointers.
    gpu::GetVulkanFunctionPointers()->vkCreateImage.OverrideForTesting(
        &MockVkCreateImage);
    gpu::GetVulkanFunctionPointers()
        ->vkGetImageMemoryRequirements.OverrideForTesting(
            &MockVkGetImageMemoryRequirements);
    gpu::GetVulkanFunctionPointers()->vkDestroyImage.OverrideForTesting(
        &MockVkDestroyImage);
    gpu::GetVulkanFunctionPointers()->vkAllocateMemory.OverrideForTesting(
        &MockVkAllocateMemory);
    gpu::GetVulkanFunctionPointers()->vkBindImageMemory.OverrideForTesting(
        &MockVkBindImageMemory);
    gpu::GetVulkanFunctionPointers()->vkFreeMemory.OverrideForTesting(
        &MockVkFreeMemory);
    gpu::GetVulkanFunctionPointers()
        ->vkGetBufferCollectionPropertiesFUCHSIA.OverrideForTesting(
            &MockVkGetBufferCollectionPropertiesFUCHSIA);
  }

  void TearDown() override {
    // Restore original Vulkan function pointers.
    gpu::GetVulkanFunctionPointers()->vkCreateImage.OverrideForTesting(
        orig_create_image_);
    gpu::GetVulkanFunctionPointers()
        ->vkGetImageMemoryRequirements.OverrideForTesting(orig_get_reqs_);
    gpu::GetVulkanFunctionPointers()->vkDestroyImage.OverrideForTesting(
        orig_destroy_image_);
    gpu::GetVulkanFunctionPointers()->vkAllocateMemory.OverrideForTesting(
        orig_alloc_mem_);
    gpu::GetVulkanFunctionPointers()->vkBindImageMemory.OverrideForTesting(
        orig_bind_mem_);
    gpu::GetVulkanFunctionPointers()->vkFreeMemory.OverrideForTesting(
        orig_free_mem_);
    gpu::GetVulkanFunctionPointers()
        ->vkGetBufferCollectionPropertiesFUCHSIA.OverrideForTesting(
            orig_get_props_);
  }

 private:
  PFN_vkCreateImage orig_create_image_;
  PFN_vkGetImageMemoryRequirements orig_get_reqs_;
  PFN_vkDestroyImage orig_destroy_image_;
  PFN_vkAllocateMemory orig_alloc_mem_;
  PFN_vkBindImageMemory orig_bind_mem_;
  PFN_vkFreeMemory orig_free_mem_;
  PFN_vkGetBufferCollectionPropertiesFUCHSIA orig_get_props_;
};

TEST_F(FlatlandSysmemBufferCollectionTest, InitializeValidation) {
  VkDevice dummy_device = reinterpret_cast<VkDevice>(1);

  {
    auto collection = base::MakeRefCounted<FlatlandSysmemBufferCollection>();
    zx::eventpair handle0, handle1;
    ASSERT_EQ(zx::eventpair::create(0, &handle0, &handle1), ZX_OK);
    zx::channel token0, token1;
    ASSERT_EQ(zx::channel::create(0, &token0, &token1), ZX_OK);

    token1.reset();  // Close peer so Sync fails immediately if it gets there.

    base::test::MockLog mock_log;
    mock_log.StartCapturingLogs();

    // Test invalid size (area overflows int).
    gfx::Size invalid_size(100000, 100000);
    EXPECT_CALL(mock_log, Log(logging::LOGGING_ERROR, _, _, _,
                              HasSubstr("Invalid size: 100000x100000")))
        .Times(1);

    bool result = collection->Initialize(
        /*sysmem_alloc=*/nullptr,
        /*register_buffer_collection=*/base::NullCallback(),
        /*flatland_surface_factory=*/nullptr, std::move(handle0),
        std::move(token0), invalid_size, viz::SinglePlaneFormat::kRGBA_8888,
        NativePixmapBufferUsage::kScanout, dummy_device,
        /*min_buffer_count=*/1);

    EXPECT_FALSE(result);
  }

  {
    auto collection = base::MakeRefCounted<FlatlandSysmemBufferCollection>();
    zx::eventpair handle0, handle1;
    ASSERT_EQ(zx::eventpair::create(0, &handle0, &handle1), ZX_OK);
    zx::channel token0, token1;
    ASSERT_EQ(zx::channel::create(0, &token0, &token1), ZX_OK);

    token1.reset();  // Close peer so Sync fails immediately.

    base::test::MockLog mock_log;
    mock_log.StartCapturingLogs();

    // Test valid size.
    gfx::Size valid_size(100, 100);
    // Should NOT log "Invalid size".
    EXPECT_CALL(mock_log,
                Log(logging::LOGGING_ERROR, _, _, _, HasSubstr("Invalid size")))
        .Times(0);
    // It might log other errors because we pass nullptr allocator, but not
    // "Invalid size".
    EXPECT_CALL(mock_log, Log(logging::LOGGING_ERROR, _, _, _, _))
        .Times(testing::AnyNumber());

    bool result = collection->Initialize(
        /*sysmem_alloc=*/nullptr,
        /*register_buffer_collection=*/base::NullCallback(),
        /*flatland_surface_factory=*/nullptr, std::move(handle0),
        std::move(token0), valid_size, viz::SinglePlaneFormat::kRGBA_8888,
        NativePixmapBufferUsage::kScanout, dummy_device,
        /*min_buffer_count=*/1);

    EXPECT_FALSE(result);  // Still expected to fail.
  }
}

TEST_F(FlatlandSysmemBufferCollectionTest, InitializeRejectsUnsupportedFormat) {
  auto collection = base::MakeRefCounted<FlatlandSysmemBufferCollection>();
  zx::eventpair handle0, handle1;
  ASSERT_EQ(zx::eventpair::create(0, &handle0, &handle1), ZX_OK);
  zx::channel token0, token1;
  ASSERT_EQ(zx::channel::create(0, &token0, &token1), ZX_OK);

  token1.reset();

  base::test::MockLog mock_log;
  mock_log.StartCapturingLogs();
  EXPECT_CALL(mock_log, Log(logging::LOGGING_ERROR, _, _, _,
                            HasSubstr("Unsupported format/usage")))
      .Times(1);

  VkDevice dummy_device = reinterpret_cast<VkDevice>(1);
  bool result = collection->Initialize(
      /*sysmem_alloc=*/nullptr,
      /*register_buffer_collection=*/base::NullCallback(),
      /*flatland_surface_factory=*/nullptr, std::move(handle0),
      std::move(token0), gfx::Size(100, 100), viz::MultiPlaneFormat::kYV12,
      NativePixmapBufferUsage::kScanout, dummy_device,
      /*min_buffer_count=*/1);

  EXPECT_FALSE(result);
}

TEST_F(FlatlandSysmemBufferCollectionTest,
       CreateNativePixmapAcceptsSizeWithinAllocatedBounds) {
  zx::eventpair client_handle;
  auto collection = MakeCollection(NativePixmapBufferUsage::kGpuRead,
                                   &client_handle, /*include_vmo=*/false);

  auto pixmap = collection->CreateNativePixmap(
      MakePixmapHandle(client_handle),
      gfx::Size(kAllocatedWidth, kAllocatedHeight));
  EXPECT_TRUE(pixmap);
}

TEST_F(FlatlandSysmemBufferCollectionTest,
       CreateNativePixmapRejectsSizeLargerThanAllocatedBounds) {
  zx::eventpair client_handle;
  auto collection = MakeCollection(NativePixmapBufferUsage::kGpuRead,
                                   &client_handle, /*include_vmo=*/false);

  base::test::MockLog mock_log;
  mock_log.StartCapturingLogs();
  EXPECT_CALL(mock_log,
              Log(logging::LOGGING_ERROR, _, _, _,
                  HasSubstr("exceeds the allocated sysmem buffer size")))
      .Times(1);

  auto pixmap = collection->CreateNativePixmap(
      MakePixmapHandle(client_handle),
      gfx::Size(kAllocatedWidth * 2, kAllocatedHeight * 2));
  EXPECT_FALSE(pixmap);
}

TEST_F(FlatlandSysmemBufferCollectionTest,
       CreateNativePixmapMappableAcceptsSizeWithinAllocatedBounds) {
  zx::eventpair client_handle;
  auto collection =
      MakeCollection(NativePixmapBufferUsage::kGpuReadCpuReadWrite,
                     &client_handle, /*include_vmo=*/true);

  auto pixmap = collection->CreateNativePixmap(
      MakePixmapHandle(client_handle),
      gfx::Size(kAllocatedWidth, kAllocatedHeight));
  EXPECT_TRUE(pixmap);
}

TEST_F(FlatlandSysmemBufferCollectionTest,
       CreateNativePixmapMappableRejectsSizeLargerThanAllocatedBounds) {
  zx::eventpair client_handle;
  auto collection =
      MakeCollection(NativePixmapBufferUsage::kGpuReadCpuReadWrite,
                     &client_handle, /*include_vmo=*/true);

  base::test::MockLog mock_log;
  mock_log.StartCapturingLogs();
  EXPECT_CALL(mock_log,
              Log(logging::LOGGING_ERROR, _, _, _,
                  HasSubstr("exceeds the allocated sysmem buffer size")))
      .Times(1);

  auto pixmap = collection->CreateNativePixmap(
      MakePixmapHandle(client_handle),
      gfx::Size(kAllocatedWidth * 2, kAllocatedHeight * 2));
  EXPECT_FALSE(pixmap);
}

TEST_F(FlatlandSysmemBufferCollectionTest, CreateVkImageAcceptsValidSize) {
  zx::eventpair client_handle;
  VkDevice dummy_device = reinterpret_cast<VkDevice>(1);
  auto collection =
      MakeCollection(NativePixmapBufferUsage::kGpuRead, &client_handle,
                     /*include_vmo=*/false, dummy_device);
  VkImage vk_image = VK_NULL_HANDLE;
  VkImageCreateInfo image_info = {};
  VkDeviceMemory vk_memory = VK_NULL_HANDLE;
  VkDeviceSize allocation_size = 0;

  g_mock_vk_image_requirements_size = kAllocatedBufferBytes;

  bool result = collection->CreateVkImage(
      0, dummy_device, gfx::Size(kAllocatedWidth, kAllocatedHeight), &vk_image,
      &image_info, &vk_memory, &allocation_size);

  EXPECT_TRUE(result);
  EXPECT_NE(vk_image, VK_NULL_HANDLE);
  EXPECT_NE(vk_memory, VK_NULL_HANDLE);
  EXPECT_EQ(allocation_size, kAllocatedBufferBytes);
}

TEST_F(FlatlandSysmemBufferCollectionTest, CreateVkImageRejectsOversizedImage) {
  zx::eventpair client_handle;
  VkDevice dummy_device = reinterpret_cast<VkDevice>(1);
  auto collection =
      MakeCollection(NativePixmapBufferUsage::kGpuRead, &client_handle,
                     /*include_vmo=*/false, dummy_device);
  VkImage vk_image = VK_NULL_HANDLE;
  VkImageCreateInfo image_info = {};
  VkDeviceMemory vk_memory = VK_NULL_HANDLE;
  VkDeviceSize allocation_size = 0;

  base::test::MockLog mock_log;
  mock_log.StartCapturingLogs();
  EXPECT_CALL(mock_log,
              Log(logging::LOGGING_ERROR, _, _, _,
                  HasSubstr("exceeds the allocated sysmem buffer size")))
      .Times(1);

  bool result = collection->CreateVkImage(
      0, dummy_device, gfx::Size(kAllocatedWidth * 2, kAllocatedHeight * 2),
      &vk_image, &image_info, &vk_memory, &allocation_size);

  EXPECT_FALSE(result);
}

TEST_F(FlatlandSysmemBufferCollectionTest,
       CreateVkImageAllowsVulkanRequirementMismatchWithWarning) {
  zx::eventpair client_handle;
  VkDevice dummy_device = reinterpret_cast<VkDevice>(1);
  auto collection =
      MakeCollection(NativePixmapBufferUsage::kGpuRead, &client_handle,
                     /*include_vmo=*/false, dummy_device);
  VkImage vk_image = VK_NULL_HANDLE;
  VkImageCreateInfo image_info = {};
  VkDeviceMemory vk_memory = VK_NULL_HANDLE;
  VkDeviceSize allocation_size = 0;

  // Set mock Vulkan requirements to exceed the allocated buffer size.
  g_mock_vk_image_requirements_size = kAllocatedBufferBytes + 1024;

  base::test::MockLog mock_log;
  mock_log.StartCapturingLogs();
  // We expect a warning log but NOT a crash.
  EXPECT_CALL(mock_log,
              Log(logging::LOGGING_WARNING, _, _, _,
                  HasSubstr("exceed the allocated sysmem buffer size")))
      .Times(1);

  bool result = collection->CreateVkImage(
      0, dummy_device, gfx::Size(kAllocatedWidth, kAllocatedHeight), &vk_image,
      &image_info, &vk_memory, &allocation_size);

  EXPECT_TRUE(result);
  EXPECT_NE(vk_image, VK_NULL_HANDLE);
  EXPECT_NE(vk_memory, VK_NULL_HANDLE);
  EXPECT_EQ(allocation_size, kAllocatedBufferBytes + 1024);
}

}  // namespace ui
