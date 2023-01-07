// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_sysmem_buffer_manager.h"

#include <zircon/rights.h>

#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/hash/hash.h"
#include "ui/ozone/platform/flatland/flatland_sysmem_buffer_collection.h"

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
    fuchsia::sysmem::AllocatorHandle sysmem_allocator,
    fuchsia::ui::composition::AllocatorHandle flatland_allocator) {
  base::AutoLock auto_lock(collections_lock_);
  DCHECK(collections_.empty());

  DCHECK(!sysmem_allocator_);
  sysmem_allocator_.Bind(std::move(sysmem_allocator));
  sysmem_allocator_->SetDebugClientInfo(
      GetProcessName() + "-FlatlandSysmemBufferManager",
      base::GetCurrentProcId());

  DCHECK(!flatland_allocator_);
  flatland_allocator_.Bind(std::move(flatland_allocator));
  flatland_allocator_.set_error_handler(base::LogFidlErrorAndExitProcess(
      FROM_HERE, "fuchsia::ui::composition::Allocator"));
}

void FlatlandSysmemBufferManager::Shutdown() {
  base::AutoLock auto_lock(collections_lock_);
  DCHECK(collections_.empty());
  sysmem_allocator_ = nullptr;
  flatland_allocator_ = nullptr;
}

scoped_refptr<FlatlandSysmemBufferCollection>
FlatlandSysmemBufferManager::CreateCollection(VkDevice vk_device,
                                              gfx::Size size,
                                              gfx::BufferFormat format,
                                              gfx::BufferUsage usage,
                                              size_t min_buffer_count) {
  auto result = base::MakeRefCounted<FlatlandSysmemBufferCollection>();
  if (!result->Initialize(sysmem_allocator_.get(), flatland_allocator_.get(),
                          flatland_surface_factory_,
                          /*token_channel=*/zx::channel(), size, format, usage,
                          vk_device, min_buffer_count)) {
    return nullptr;
  }
  RegisterCollection(result.get());
  return result;
}

scoped_refptr<FlatlandSysmemBufferCollection>
FlatlandSysmemBufferManager::ImportFlatlandSysmemBufferCollection(
    VkDevice vk_device,
    gfx::SysmemBufferCollectionId id,
    zx::channel token,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    size_t min_buffer_count) {
  auto result = base::MakeRefCounted<FlatlandSysmemBufferCollection>(id);
  if (!result->Initialize(sysmem_allocator_.get(), flatland_allocator_.get(),
                          flatland_surface_factory_, std::move(token), size,
                          format, usage, vk_device, min_buffer_count)) {
    return nullptr;
  }
  RegisterCollection(result.get());
  return result;
}

void FlatlandSysmemBufferManager::RegisterCollection(
    FlatlandSysmemBufferCollection* collection) {
  {
    base::AutoLock auto_lock(collections_lock_);
    collections_[collection->id()] = collection;
  }

  collection->AddOnDeletedCallback(
      base::BindOnce(&FlatlandSysmemBufferManager::OnCollectionDestroyed,
                     base::Unretained(this), collection->id()));
}

scoped_refptr<FlatlandSysmemBufferCollection>
FlatlandSysmemBufferManager::GetCollectionById(
    gfx::SysmemBufferCollectionId id) {
  base::AutoLock auto_lock(collections_lock_);
  auto it = collections_.find(id);
  return it == collections_.end() ? nullptr : it->second;
}

void FlatlandSysmemBufferManager::OnCollectionDestroyed(
    gfx::SysmemBufferCollectionId id) {
  base::AutoLock auto_lock(collections_lock_);
  int erased = collections_.erase(id);
  DCHECK_EQ(erased, 1);
}

}  // namespace ui
