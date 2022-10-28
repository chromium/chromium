// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/sysmem_buffer_manager.h"

#include <lib/zx/eventpair.h>
#include <zircon/rights.h>
#include <zircon/types.h>

#include "base/containers/contains.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/koid.h"
#include "base/functional/bind.h"
#include "ui/gfx/native_pixmap_handle.h"
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

scoped_refptr<gfx::NativePixmap> SysmemBufferManager::CreateNativePixmap(
    VkDevice vk_device,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  gfx::NativePixmapHandle pixmap_handle;
  zx::eventpair service_handle;
  auto status = zx::eventpair::create(
      0, &pixmap_handle.buffer_collection_handle, &service_handle);
  ZX_DCHECK(status == ZX_OK, status);

  auto collection = base::MakeRefCounted<SysmemBufferCollection>();
  if (!collection->Initialize(allocator_.get(), scenic_surface_factory_,
                              std::move(service_handle),
                              /*token_channel=*/zx::channel(), size, format,
                              usage, vk_device, /*min_buffer_count=*/1,
                              /*register_with_image_pipe=*/false)) {
    return nullptr;
  }

  auto result = collection->CreateNativePixmap(std::move(pixmap_handle), size);

  if (result)
    RegisterCollection(collection);

  return result;
}

scoped_refptr<SysmemBufferCollection>
SysmemBufferManager::ImportSysmemBufferCollection(
    VkDevice vk_device,
    zx::eventpair service_handle,
    zx::channel sysmem_token,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    size_t min_buffer_count,
    bool register_with_image_pipe) {
  auto result = base::MakeRefCounted<SysmemBufferCollection>();
  if (!result->Initialize(allocator_.get(), scenic_surface_factory_,
                          std::move(service_handle), std::move(sysmem_token),
                          size, format, usage, vk_device, min_buffer_count,
                          register_with_image_pipe)) {
    return nullptr;
  }
  RegisterCollection(result);
  return result;
}

void SysmemBufferManager::RegisterCollection(
    scoped_refptr<SysmemBufferCollection> collection) {
  {
    base::AutoLock auto_lock(collections_lock_);
    DCHECK(!base::Contains(collections_, collection->id()));
    collections_[collection->id()] = collection;
  }

  collection->AddOnReleasedCallback(
      base::BindOnce(&SysmemBufferManager::OnCollectionReleased,
                     base::Unretained(this), collection->id()));
}

scoped_refptr<SysmemBufferCollection>
SysmemBufferManager::GetCollectionByHandle(const zx::eventpair& token) {
  auto koid = base::GetRelatedKoid(token);
  if (!koid)
    return nullptr;

  base::AutoLock auto_lock(collections_lock_);
  auto it = collections_.find(koid.value());
  return it == collections_.end() ? nullptr : it->second;
}

void SysmemBufferManager::OnCollectionReleased(zx_koid_t id) {
  base::AutoLock auto_lock(collections_lock_);
  int erased = collections_.erase(id);
  DCHECK_EQ(erased, 1);
}

}  // namespace ui
