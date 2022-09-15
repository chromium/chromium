// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/sysmem_buffer_manager.h"

#include <zircon/rights.h>

#include "base/bind.h"
#include "base/hash/hash.h"
#include "ui/ozone/platform/scenic/sysmem_buffer_collection.h"

namespace ui {

namespace {

std::string GetProcessName() {
  char name[ZX_MAX_NAME_LEN] = {};
  zx_status_t status =
      zx::process::self()->get_property(ZX_PROP_NAME, name, sizeof(name));
  return (status == ZX_OK) ? std::string(name) : "";
}

}  // namespace

SysmemBufferManager::SysmemBufferManager(
    ScenicSurfaceFactory* scenic_surface_factory)
    : scenic_surface_factory_(scenic_surface_factory) {}

SysmemBufferManager::~SysmemBufferManager() {
  Shutdown();
}

void SysmemBufferManager::Initialize(
    fuchsia::sysmem::AllocatorHandle allocator) {
  base::AutoLock auto_lock(collections_lock_);
  DCHECK(collections_.empty());
  DCHECK(!allocator_);
  allocator_.Bind(std::move(allocator));
  allocator_->SetDebugClientInfo(GetProcessName() + "-SysmemBufferManager",
                                 base::GetCurrentProcId());
}

void SysmemBufferManager::Shutdown() {
  base::AutoLock auto_lock(collections_lock_);
  DCHECK(collections_.empty());
  allocator_ = nullptr;
}

fuchsia::sysmem::Allocator_Sync* SysmemBufferManager::GetAllocator() {
  DCHECK(allocator_);
  return allocator_.get();
}

scoped_refptr<SysmemBufferCollection> SysmemBufferManager::CreateCollection(
    VkDevice vk_device,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    size_t min_buffer_count) {
  auto result = base::MakeRefCounted<SysmemBufferCollection>();
  if (!result->Initialize(allocator_.get(), scenic_surface_factory_,
                          /*token_channel=*/zx::channel(), size, format, usage,
                          vk_device, min_buffer_count,
                          /*register_with_image_pipe=*/false)) {
    return nullptr;
  }
  RegisterCollection(result.get());
  return result;
}

scoped_refptr<SysmemBufferCollection>
SysmemBufferManager::ImportSysmemBufferCollection(
    VkDevice vk_device,
    gfx::SysmemBufferCollectionId id,
    zx::channel token,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    size_t min_buffer_count,
    bool register_with_image_pipe) {
  auto result = base::MakeRefCounted<SysmemBufferCollection>(id);
  if (!result->Initialize(allocator_.get(), scenic_surface_factory_,
                          std::move(token), size, format, usage, vk_device,
                          min_buffer_count, register_with_image_pipe)) {
    return nullptr;
  }
  RegisterCollection(result.get());
  return result;
}

void SysmemBufferManager::RegisterCollection(
    SysmemBufferCollection* collection) {
  {
    base::AutoLock auto_lock(collections_lock_);
    collections_[collection->id()] = collection;
  }

  collection->AddOnDeletedCallback(
      base::BindOnce(&SysmemBufferManager::OnCollectionDestroyed,
                     base::Unretained(this), collection->id()));
}

scoped_refptr<SysmemBufferCollection> SysmemBufferManager::GetCollectionById(
    gfx::SysmemBufferCollectionId id) {
  base::AutoLock auto_lock(collections_lock_);
  auto it = collections_.find(id);
  return it == collections_.end() ? nullptr : it->second;
}

void SysmemBufferManager::OnCollectionDestroyed(
    gfx::SysmemBufferCollectionId id) {
  base::AutoLock auto_lock(collections_lock_);
  int erased = collections_.erase(id);
  DCHECK_EQ(erased, 1);
}

}  // namespace ui
