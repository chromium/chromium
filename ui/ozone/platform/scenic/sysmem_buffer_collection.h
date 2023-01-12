// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_SCENIC_SYSMEM_BUFFER_COLLECTION_H_
#define UI_OZONE_PLATFORM_SCENIC_SYSMEM_BUFFER_COLLECTION_H_

#include <fuchsia/sysmem/cpp/fidl.h>
#include <lib/ui/scenic/cpp/session.h>
#include <lib/zx/eventpair.h>
#include <vulkan/vulkan_core.h>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_for_io.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/ozone/platform/scenic/scenic_overlay_view.h"

namespace gfx {
class NativePixmap;
}  // namespace gfx

namespace ui {

class ScenicSurfaceFactory;

// SysmemBufferCollection keeps sysmem.BufferCollection interface along with the
// corresponding VkBufferCollectionFUCHSIA. It allows to create either
// gfx::NativePixmap or VkImage from the buffers in the collection.
// A collection can be initialized and used on any thread.  CreateVkImage() must
// be called on the same thread (because it may be be safe to use
// VkBufferCollectionFUCHSIA concurrently on different threads).
class SysmemBufferCollection
    : public base::RefCountedThreadSafe<SysmemBufferCollection>,
      public base::MessagePumpForIO::ZxHandleWatcher {
 public:
  static bool IsNativePixmapConfigSupported(gfx::BufferFormat format,
                                            gfx::BufferUsage usage);

  SysmemBufferCollection();

  SysmemBufferCollection(const SysmemBufferCollection&) = delete;
  SysmemBufferCollection& operator=(const SysmemBufferCollection&) = delete;

  // Initializes the buffer collection and registers it with Vulkan using the
  // specified |vk_device|. If |token_handle| is null then a new collection
  // collection is created. |size| may be empty. In that case |token_handle|
  // must not be null and the image size is determined by the other sysmem
  // participants.
  // If |register_with_image_pipe| is true, new ScenicOverlayView instance is
  // created and |token_handle| gets duplicated to be added to its ImagePipe.
  bool Initialize(fuchsia::sysmem::Allocator_Sync* allocator,
                  ScenicSurfaceFactory* scenic_surface_factory,
                  zx::eventpair handle,
                  zx::channel sysmem_token,
                  gfx::Size size,
                  gfx::BufferFormat format,
                  gfx::BufferUsage usage,
                  VkDevice vk_device,
                  size_t min_buffer_count,
                  bool register_with_image_pipe);

  void AddOnReleasedCallback(base::OnceClosure on_released);

  // Creates a NativePixmap the buffer with the specified index. Returned
  // NativePixmap holds a reference to the collection, so the collection is not
  // deleted until all NativePixmap are destroyed.
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
  size_t num_buffers() const { return buffers_info_.buffer_count; }
  gfx::BufferFormat format() const { return format_; }
  size_t buffer_size() const {
    return buffers_info_.settings.buffer_settings.size_bytes;
  }
  ScenicOverlayView* scenic_overlay_view() {
    return scenic_overlay_view_ ? scenic_overlay_view_.get() : nullptr;
  }

 private:
  friend class base::RefCountedThreadSafe<SysmemBufferCollection>;

  ~SysmemBufferCollection() override;

  bool InitializeInternal(
      fuchsia::sysmem::Allocator_Sync* allocator,
      fuchsia::sysmem::BufferCollectionTokenSyncPtr collection_token,
      size_t buffers_for_camping);

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

  fuchsia::sysmem::BufferCollectionSyncPtr collection_;
  fuchsia::sysmem::BufferCollectionInfo_2 buffers_info_;

  // Vulkan device for which the collection was initialized.
  VkDevice vk_device_ = VK_NULL_HANDLE;

  // Handle for the Vulkan object that holds the same logical buffer collection
  // that is referenced by |collection_|.
  VkBufferCollectionFUCHSIA vk_buffer_collection_ = VK_NULL_HANDLE;

  // |scenic_overlay_view_| view should be used and deleted on the same thread
  // as creation.
  scoped_refptr<base::SingleThreadTaskRunner> overlay_view_task_runner_;
  // If ScenicOverlayView is created and its ImagePipe is added as a participant
  // in buffer allocation negotiations, the associated images can be displayed
  // as overlays.
  std::unique_ptr<ScenicOverlayView> scenic_overlay_view_;

  // Thread checker used to verify that CreateVkImage() is always called from
  // the same thread. It may be unsafe to use vk_buffer_collection_ on different
  // threads.
  THREAD_CHECKER(vulkan_thread_checker_);

  size_t buffer_size_ = 0;
  bool is_protected_ = false;

  std::vector<base::OnceClosure> on_released_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_SCENIC_SYSMEM_BUFFER_COLLECTION_H_
