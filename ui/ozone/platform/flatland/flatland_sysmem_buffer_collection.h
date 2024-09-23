// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_FLATLAND_FLATLAND_SYSMEM_BUFFER_COLLECTION_H_
#define UI_OZONE_PLATFORM_FLATLAND_FLATLAND_SYSMEM_BUFFER_COLLECTION_H_

#include <fuchsia/sysmem/cpp/fidl.h>
#include <fuchsia/ui/composition/cpp/fidl.h>
#include <lib/zx/eventpair.h>
#include <vulkan/vulkan.h>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/threading/thread_checker.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace gfx {
class NativePixmap;
}  // namespace gfx

namespace ui {

class FlatlandSurfaceFactory;

// FlatlandSysmemBufferCollection keeps sysmem.BufferCollection interface along
// with the corresponding VkBufferCollectionFUCHSIA. It allows to create either
// gfx::NativePixmap or VkImage from the buffers in the collection.
// A collection can be initialized and used on any thread.  CreateVkImage() must
// be called on the same thread (because it may be be safe to use
// VkBufferCollectionFUCHSIA concurrently on different threads).
class FlatlandSysmemBufferCollection
    : public base::RefCountedThreadSafe<FlatlandSysmemBufferCollection>,
      public base::MessagePumpForIO::ZxHandleWatcher {
 public:
  static bool IsNativePixmapConfigSupported(gfx::BufferFormat format,
                                            gfx::BufferUsage usage);

  FlatlandSysmemBufferCollection();
  FlatlandSysmemBufferCollection(const FlatlandSysmemBufferCollection&) =
      delete;
  FlatlandSysmemBufferCollection& operator=(
      const FlatlandSysmemBufferCollection&) = delete;

  // Initializes the buffer collection and registers it with Vulkan using the
  // specified |vk_device|. If |token_handle| is null then a new collection
  // collection is created. |size| may be empty. In that case |token_handle|
  // must not be null and the image size is determined by the other sysmem
  // participants.
  // If |register_with_flatland_allocator| is true, |token_handle| gets
  // duplicated and registered with the Flatland Allocator.
  bool Initialize(fuchsia::sysmem2::Allocator_Sync* sysmem_allocator,
                  fuchsia::ui::composition::Allocator* flatland_allocator,
                  FlatlandSurfaceFactory* flatland_surface_factory,
                  zx::eventpair handle,
                  zx::channel sysmem_token,
                  gfx::Size size,
                  gfx::BufferFormat format,
                  gfx::BufferUsage usage,
                  VkDevice vk_device,
                  size_t min_buffer_count,
                  bool register_with_flatland_allocator);

  // Does minimum initialization needed for tests based on |usage|.
  void InitializeForTesting(zx::eventpair handle, gfx::BufferUsage usage);

  // Creates a NativePixmap with the specified handle. The handle must reference
  // a buffer in this collection.
  scoped_refptr<gfx::NativePixmap> CreateNativePixmap(
      gfx::NativePixmapHandle handle,
      gfx::Size size);

  // Creates a new Vulkan image for the buffer with the specified index.
  bool CreateVkImage(size_t buffer_index,
                     VkDevice vk_device,
                     gfx::Size size,
                     VkImage* vk_image,
                     VkImageCreateInfo* vk_image_info,
                     VkDeviceMemory* vk_device_memory,
                     VkDeviceSize* mem_allocation_size);

  zx_koid_t id() const { return id_; }
  size_t num_buffers() const { return buffers_info_.buffers().size(); }
  gfx::BufferFormat format() const { return format_; }
  size_t buffer_size() const {
    return buffers_info_.settings().buffer_settings().size_bytes();
  }

  // Returns a duplicate of |flatland_import_token_| so Images can be created.
  fuchsia::ui::composition::BufferCollectionImportToken GetFlatlandImportToken()
      const;
  bool HasFlatlandImportToken() const;

  void AddOnReleasedCallback(base::OnceClosure on_released);

 private:
  friend class base::RefCountedThreadSafe<FlatlandSysmemBufferCollection>;

  ~FlatlandSysmemBufferCollection() override;

  bool InitializeInternal(
      fuchsia::sysmem2::Allocator_Sync* sysmem_allocator,
      fuchsia::ui::composition::Allocator* flatland_allocator,
      fuchsia::sysmem2::BufferCollectionTokenSyncPtr collection_token,
      bool register_with_flatland_allocator,
      size_t min_buffer_count);

  void InitializeImageCreateInfo(VkImageCreateInfo* vk_image_info,
                                 gfx::Size size);

  bool is_mappable() const {
    return usage_ == gfx::BufferUsage::SCANOUT_CPU_READ_WRITE ||
           usage_ == gfx::BufferUsage::GPU_READ_CPU_READ_WRITE;
  }

  // base::MessagePumpForIO::ZxHandleWatcher implementation.
  void OnZxHandleSignalled(zx_handle_t handle, zx_signals_t signals) override;

  zx::eventpair handle_;
  zx_koid_t id_ = 0;

  std::unique_ptr<base::MessagePumpForIO::ZxHandleWatchController>
      handle_watch_;

  // Image size passed to vkSetBufferCollectionConstraintsFUCHSIA(). The actual
  // buffers size may be larger depending on constraints set by other
  // sysmem clients. Size of the image is passed to CreateVkImage().
  gfx::Size min_size_;

  gfx::BufferFormat format_ = gfx::BufferFormat::RGBA_8888;
  gfx::BufferUsage usage_ = gfx::BufferUsage::GPU_READ_CPU_READ_WRITE;

  fuchsia::sysmem2::BufferCollectionSyncPtr collection_;
  fuchsia::sysmem2::BufferCollectionInfo buffers_info_;
  fuchsia::ui::composition::BufferCollectionImportToken flatland_import_token_;

  // Vulkan device for which the collection was initialized.
  VkDevice vk_device_ = VK_NULL_HANDLE;

  // Handle for the Vulkan object that holds the same logical buffer collection
  // that is referenced by |collection_|.
  VkBufferCollectionFUCHSIA vk_buffer_collection_ = VK_NULL_HANDLE;

  // Thread checker used to verify that CreateVkImage() is always called from
  // the same thread. It may be unsafe to use vk_buffer_collection_ on different
  // threads.
  THREAD_CHECKER(vulkan_thread_checker_);

  size_t buffer_size_ = 0;
  bool is_protected_ = false;

  std::vector<base::OnceClosure> on_released_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_FLATLAND_FLATLAND_SYSMEM_BUFFER_COLLECTION_H_
