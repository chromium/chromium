// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_sysmem_buffer_manager.h"

#include <zircon/rights.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/koid.h"
#include "base/functional/bind.h"
#include "components/viz/common/resources/shared_image_format_utils.h"
#include "ui/ozone/platform/flatland/flatland_sysmem_buffer_collection.h"
#include "ui/ozone/public/native_pixmap_usage_utils.h"

namespace ui {

namespace {

std::string GetProcessName() {
  char name[ZX_MAX_NAME_LEN] = {};
  zx_status_t status =
      zx::process::self()->get_property(ZX_PROP_NAME, name, sizeof(name));
  return (status == ZX_OK) ? std::string(name) : "";
}

}  // namespace

FlatlandSysmemBufferManager::FlatlandSysmemBufferManager(
    FlatlandSurfaceFactory* flatland_surface_factory)
    : flatland_surface_factory_(flatland_surface_factory) {}

FlatlandSysmemBufferManager::~FlatlandSysmemBufferManager() {
  Shutdown();
}

void FlatlandSysmemBufferManager::Initialize(
    fuchsia::sysmem2::AllocatorHandle sysmem_allocator,
    fuchsia::ui::composition::AllocatorHandle flatland_allocator) {
  base::AutoLock auto_lock(collections_lock_);
  DCHECK(collections_.empty());

  DCHECK(!sysmem_allocator_);
  sysmem_allocator_.Bind(std::move(sysmem_allocator));
  sysmem_allocator_->SetDebugClientInfo(
      std::move(fuchsia::sysmem2::AllocatorSetDebugClientInfoRequest{}
                    .set_name(GetProcessName() + "-FlatlandSysmemBufferManager")
                    .set_id(base::GetCurrentProcId())));

  DCHECK(!flatland_allocator_);
  flatland_allocator_.Bind(std::move(flatland_allocator));
  flatland_allocator_.set_error_handler(base::LogFidlErrorAndExitProcess(
      FROM_HERE, "fuchsia::ui::composition::Allocator"));
  allocator_task_runner_ = base::SingleThreadTaskRunner::GetCurrentDefault();
}

void FlatlandSysmemBufferManager::Shutdown() {
  base::AutoLock auto_lock(collections_lock_);
  DCHECK(collections_.empty());
  sysmem_allocator_ = nullptr;
  flatland_allocator_ = nullptr;
}

scoped_refptr<gfx::NativePixmap>
FlatlandSysmemBufferManager::CreateNativePixmap(VkDevice vk_device,
                                                gfx::Size size,
                                                viz::SharedImageFormat format,
                                                NativePixmapUsageSet usage) {
  gfx::NativePixmapHandle pixmap_handle;
  zx::eventpair service_handle;
  auto status = zx::eventpair::create(
      0, &pixmap_handle.buffer_collection_handle, &service_handle);
  ZX_DCHECK(status == ZX_OK, status);

  // Scanout images must be registered with flatland.
  FlatlandSysmemBufferCollection::RegisterBufferCollectionCallback register_cb;
  if (usage == NativePixmapBufferUsage::kScanout) {
    register_cb = base::BindOnce(
        &FlatlandSysmemBufferManager::RegisterWithFlatlandAllocator,
        base::Unretained(this));
  }

  auto collection = base::MakeRefCounted<FlatlandSysmemBufferCollection>();
  if (!collection->Initialize(
          sysmem_allocator_.get(), std::move(register_cb),
          flatland_surface_factory_, std::move(service_handle),
          /*token_channel=*/zx::channel(), size, format, usage, vk_device,
          /*min_buffer_count=*/1)) {
    return nullptr;
  }

  auto result = collection->CreateNativePixmap(std::move(pixmap_handle), size);

  if (result)
    RegisterCollection(collection);

  return result;
}

void FlatlandSysmemBufferManager::ImportSysmemBufferCollection(
    VkDevice vk_device,
    zx::eventpair service_handle,
    zx::channel sysmem_token,
    gfx::Size size,
    viz::SharedImageFormat format,
    gfx::BufferUsage usage,
    size_t min_buffer_count,
    bool register_with_flatland_allocator) {
  auto koid = base::GetKoid(service_handle);
  if (!koid) {
    return;
  }
  {
    base::AutoLock auto_lock(collections_lock_);
    if (collections_.contains(koid.value())) {
      return;
    }
    collections_[koid.value()] = nullptr;
  }
  NativePixmapUsageSet native_pixmap_usage =
      BufferUsageToNativePixmapUsage(usage);

  FlatlandSysmemBufferCollection::RegisterBufferCollectionCallback register_cb;
  if (register_with_flatland_allocator) {
    register_cb = base::BindOnce(
        &FlatlandSysmemBufferManager::RegisterWithFlatlandAllocator,
        base::Unretained(this));
  }

  auto result = base::MakeRefCounted<FlatlandSysmemBufferCollection>();
  if (!result->Initialize(sysmem_allocator_.get(), std::move(register_cb),
                          flatland_surface_factory_, std::move(service_handle),
                          std::move(sysmem_token), size, format,
                          native_pixmap_usage, vk_device, min_buffer_count)) {
    base::AutoLock auto_lock(collections_lock_);
    collections_.erase(koid.value());
    return;
  }
  RegisterCollection(result);
}

void FlatlandSysmemBufferManager::RegisterWithFlatlandAllocator(
    fuchsia::ui::composition::RegisterBufferCollectionArgs args) {
  DCHECK(allocator_task_runner_);
  if (!allocator_task_runner_->BelongsToCurrentThread()) {
    allocator_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &FlatlandSysmemBufferManager::RegisterWithFlatlandAllocator,
            base::Unretained(this), std::move(args)));
    return;
  }

  if (!flatland_allocator_) {
    return;
  }
  flatland_allocator_->RegisterBufferCollection(
      std::move(args),
      [](fuchsia::ui::composition::Allocator_RegisterBufferCollection_Result
             result) {
        if (result.is_err()) {
          LOG(FATAL) << "RegisterBufferCollection failed";
        }
      });
}

void FlatlandSysmemBufferManager::RegisterCollection(
    scoped_refptr<FlatlandSysmemBufferCollection> collection) {
  {
    base::AutoLock auto_lock(collections_lock_);
    auto it = collections_.find(collection->id());
    if (it != collections_.end()) {
      // The collection should only already exist if it was pre-registered as
      // nullptr by ImportSysmemBufferCollection().
      CHECK(!it->second);
      it->second = collection;
    } else {
      collections_[collection->id()] = collection;
    }
  }

  collection->AddOnReleasedCallback(
      base::BindOnce(&FlatlandSysmemBufferManager::OnCollectionReleased,
                     base::Unretained(this), collection->id()));
}

scoped_refptr<FlatlandSysmemBufferCollection>
FlatlandSysmemBufferManager::GetCollectionByHandle(const zx::eventpair& token) {
  auto koid = base::GetRelatedKoid(token);
  if (!koid)
    return nullptr;

  base::AutoLock auto_lock(collections_lock_);
  auto it = collections_.find(koid.value());
  return (it == collections_.end() || !it->second) ? nullptr : it->second;
}

void FlatlandSysmemBufferManager::OnCollectionReleased(zx_koid_t id) {
  base::AutoLock auto_lock(collections_lock_);
  int erased = collections_.erase(id);
  DCHECK_EQ(erased, 1);
}

}  // namespace ui
