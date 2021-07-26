// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/flatland_sysmem_buffer_manager.h"

#include <zircon/rights.h>

#include "base/bind.h"
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
    fuchsia::sysmem::AllocatorHandle allocator) {
  base::AutoLock auto_lock(collections_lock_);
  DCHECK(collections_.empty());
  DCHECK(!allocator_);
  allocator_.Bind(std::move(allocator));
  allocator_->SetDebugClientInfo(
      GetProcessName() + "-FlatlandSysmemBufferManager",
      base::GetCurrentProcId());
}

void FlatlandSysmemBufferManager::Shutdown() {
  base::AutoLock auto_lock(collections_lock_);
  DCHECK(collections_.empty());
  allocator_ = nullptr;
}

scoped_refptr<FlatlandSysmemBufferCollection>
FlatlandSysmemBufferManager::CreateCollection(VkDevice vk_device,
                                              gfx::Size size,
                                              gfx::BufferFormat format,
                                              gfx::BufferUsage usage,
                                              size_t min_buffer_count) {
  auto result = base::MakeRefCounted<FlatlandSysmemBufferCollection>();
  if (!result->Initialize(allocator_.get(), flatland_surface_factory_,
                          /*token_channel=*/zx::channel(), size, format, usage,
                          vk_device, min_buffer_count,
                          /*register_with_image_pipe=*/false)) {
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
    size_t min_buffer_count,
    bool register_with_image_pipe) {
  auto result = base::MakeRefCounted<FlatlandSysmemBufferCollection>(id);
  if (!result->Initialize(allocator_.get(), flatland_surface_factory_,
                          std::move(token), size, format, usage, vk_device,
                          min_buffer_count, register_with_image_pipe)) {
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

  collection->SetOnDeletedCallback(
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
