// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_FLATLAND_FLATLAND_SYSMEM_BUFFER_MANAGER_H_
#define UI_OZONE_PLATFORM_FLATLAND_FLATLAND_SYSMEM_BUFFER_MANAGER_H_

#include <fuchsia/sysmem/cpp/fidl.h>
#include <fuchsia/ui/composition/cpp/fidl.h>
#include <lib/zx/channel.h>
#include <lib/zx/eventpair.h>
#include <vulkan/vulkan_core.h>

#include <unordered_map>

#include "base/containers/small_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/unguessable_token.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap.h"

namespace ui {

class FlatlandSysmemBufferCollection;
class FlatlandSurfaceFactory;

class FlatlandSysmemBufferManager {
 public:
  explicit FlatlandSysmemBufferManager(
      FlatlandSurfaceFactory* flatland_surface_factory);
  ~FlatlandSysmemBufferManager();
  FlatlandSysmemBufferManager(const FlatlandSysmemBufferManager&) = delete;
  FlatlandSysmemBufferManager& operator=(const FlatlandSysmemBufferManager&) =
      delete;

  // Initializes the buffer manager with a connection to the Sysmem service and
  // Flatland Allocator.
  void Initialize(fuchsia::sysmem2::AllocatorHandle sysmem_allocator,
                  fuchsia::ui::composition::AllocatorHandle flatland_allocator);

  // Disconnects from the sysmem service. After disconnecting, it's safe to call
  // Initialize() again.
  void Shutdown();

  scoped_refptr<gfx::NativePixmap> CreateNativePixmap(VkDevice vk_device,
                                                      gfx::Size size,
                                                      gfx::BufferFormat format,
                                                      gfx::BufferUsage usage);

  // TODO(crbug.com/42050538): Instead of an additional
  // |register_with_flatland_allocator| bool, we can rely on |usage| to decide
  // if the buffers should be registered with Flatland or not.
  scoped_refptr<FlatlandSysmemBufferCollection> ImportSysmemBufferCollection(
      VkDevice vk_device,
      zx::eventpair service_handle,
      zx::channel sysmem_token,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      size_t min_buffer_count,
      bool register_with_flatland_allocator);

  // Returns `SysmemBufferCollection` that corresponds to the specified
  // buffer collection `handle`, which should be the other end of the eventpair
  // passed to `ImportSysmemBufferCollection()`.
  scoped_refptr<FlatlandSysmemBufferCollection> GetCollectionByHandle(
      const zx::eventpair& handle);

  fuchsia::sysmem2::Allocator_Sync* sysmem_allocator() {
    return sysmem_allocator_.get();
  }

  fuchsia::ui::composition::Allocator* flatland_allocator() {
    return flatland_allocator_.get();
  }

 private:
  void RegisterCollection(
      scoped_refptr<FlatlandSysmemBufferCollection> collection);

  void OnCollectionReleased(zx_koid_t id);

  FlatlandSurfaceFactory* const flatland_surface_factory_;
  fuchsia::sysmem2::AllocatorSyncPtr sysmem_allocator_;
  fuchsia::ui::composition::AllocatorPtr flatland_allocator_;

  base::small_map<
      std::unordered_map<zx_koid_t,
                         scoped_refptr<FlatlandSysmemBufferCollection>>>
      collections_ GUARDED_BY(collections_lock_);
  base::Lock collections_lock_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_FLATLAND_FLATLAND_SYSMEM_BUFFER_MANAGER_H_
