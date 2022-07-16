// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_FLATLAND_FLATLAND_SYSMEM_BUFFER_MANAGER_H_
#define UI_OZONE_PLATFORM_FLATLAND_FLATLAND_SYSMEM_BUFFER_MANAGER_H_

#include <fuchsia/sysmem/cpp/fidl.h>
#include <fuchsia/ui/composition/cpp/fidl.h>
#include <vulkan/vulkan_core.h>

#include <unordered_map>

#include "base/containers/small_map.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/unguessable_token.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_pixmap_handle.h"

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
  void Initialize(fuchsia::sysmem::AllocatorHandle sysmem_allocator,
                  fuchsia::ui::composition::AllocatorHandle flatland_allocator);

  // Disconnects from the sysmem service. After disconnecting, it's safe to call
  // Initialize() again.
  void Shutdown();

  scoped_refptr<FlatlandSysmemBufferCollection> CreateCollection(
      VkDevice vk_device,
      gfx::Size size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage,
      size_t num_buffers);

  scoped_refptr<FlatlandSysmemBufferCollection>
  ImportFlatlandSysmemBufferCollection(VkDevice vk_device,
                                       gfx::SysmemBufferCollectionId id,
                                       zx::channel token,
                                       gfx::Size size,
                                       gfx::BufferFormat format,
                                       gfx::BufferUsage usage,
                                       size_t min_buffer_count);

  scoped_refptr<FlatlandSysmemBufferCollection> GetCollectionById(
      gfx::SysmemBufferCollectionId id);

 private:
  void RegisterCollection(FlatlandSysmemBufferCollection* collection);
  void OnCollectionDestroyed(gfx::SysmemBufferCollectionId id);

  FlatlandSurfaceFactory* const flatland_surface_factory_;
  fuchsia::sysmem::AllocatorSyncPtr sysmem_allocator_;
  fuchsia::ui::composition::AllocatorPtr flatland_allocator_;

  base::small_map<std::unordered_map<gfx::SysmemBufferCollectionId,
                                     FlatlandSysmemBufferCollection*,
                                     base::UnguessableTokenHash>>
      collections_ GUARDED_BY(collections_lock_);
  base::Lock collections_lock_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_FLATLAND_FLATLAND_SYSMEM_BUFFER_MANAGER_H_
