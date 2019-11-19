// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/client_native_pixmap_factory_scenic.h"

#include <lib/zx/vmar.h>
#include <lib/zx/vmo.h>
#include <vector>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/system/sys_info.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/client_native_pixmap.h"
#include "ui/gfx/client_native_pixmap_factory.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace ui {

class ClientNativePixmapFuchsia : public gfx::ClientNativePixmap {
 public:
  explicit ClientNativePixmapFuchsia(gfx::NativePixmapHandle handle)
      : handle_(std::move(handle)) {
  }

  ~ClientNativePixmapFuchsia() override {
    if (mapping_)
      Unmap();
  }

  bool Map() override {
    if (mapping_)
      return true;

    if (handle_.planes.empty() || !handle_.planes[0].vmo)
      return false;

    uintptr_t addr;

    // Assume that last plane is at the end of the VMO.
    mapping_size_ = handle_.planes.back().offset + handle_.planes.back().size;

    // Verify that all planes fall within the mapped range.
    for (auto& plane : handle_.planes) {
      DCHECK_LE(plane.offset + plane.size, mapping_size_);
    }

    // Round mapping size to align with the page size.
    size_t page_size = base::SysInfo::VMAllocationGranularity();
    mapping_size_ = (mapping_size_ + page_size - 1) & ~(page_size - 1);

    zx_status_t status =
        zx::vmar::root_self()->map(0, handle_.planes[0].vmo, 0, mapping_size_,
                                   ZX_VM_PERM_READ | ZX_VM_PERM_WRITE, &addr);
    if (status != ZX_OK) {
      ZX_DLOG(ERROR, status) << "zx_vmar_map";
      return false;
    }
    mapping_ = reinterpret_cast<uint8_t*>(addr);

    return true;
  }

  void Unmap() override {
    DCHECK(mapping_);

    // Flush the CPu cache in case the GPU reads the data directly from RAM.
    if (handle_.ram_coherency) {
      zx_status_t status =
          zx_cache_flush(mapping_, mapping_size_,
                         ZX_CACHE_FLUSH_DATA | ZX_CACHE_FLUSH_INVALIDATE);
      ZX_DCHECK(status == ZX_OK, status) << "zx_cache_flush";
    }

    zx_status_t status = zx::vmar::root_self()->unmap(
        reinterpret_cast<uintptr_t>(mapping_), mapping_size_);
    ZX_DCHECK(status == ZX_OK, status) << "zx_vmar_unmap";
    mapping_ = nullptr;
  }

  void* GetMemoryAddress(size_t plane) const override {
    DCHECK_LT(plane, handle_.planes.size());
    DCHECK(mapping_);
    return mapping_ + handle_.planes[plane].offset;
  }

  int GetStride(size_t plane) const override {
    DCHECK_LT(plane, handle_.planes.size());
    return handle_.planes[plane].stride;
  }

 private:
  gfx::NativePixmapHandle handle_;

  uint8_t* mapping_ = nullptr;
  size_t mapping_size_ = 0;

  DISALLOW_COPY_AND_ASSIGN(ClientNativePixmapFuchsia);
};

class ScenicClientNativePixmapFactory : public gfx::ClientNativePixmapFactory {
 public:
  ScenicClientNativePixmapFactory() = default;
  ~ScenicClientNativePixmapFactory() override = default;

  std::unique_ptr<gfx::ClientNativePixmap> ImportFromHandle(
      gfx::NativePixmapHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override {
    // |planes| may be empty for non-mappable pixmaps. No need to validate the
    // handle in that case.
    if (handle.planes.empty())
      return std::make_unique<ClientNativePixmapFuchsia>(std::move(handle));

    size_t expected_planes = gfx::NumberOfPlanesForLinearBufferFormat(format);
    if (handle.planes.size() != expected_planes)
      return nullptr;

    base::CheckedNumeric<size_t> vmo_size_checked =
        base::CheckedNumeric<size_t>(handle.planes.back().offset) +
        handle.planes.back().size;
    if (!vmo_size_checked.IsValid()) {
      return nullptr;
    }
    size_t vmo_size = vmo_size_checked.ValueOrDie();

    // Validate plane layout and buffer size.
    for (size_t i = 0; i < handle.planes.size(); ++i) {
      size_t min_stride = 0;
      size_t subsample_factor = SubsamplingFactorForBufferFormat(format, i);
      base::CheckedNumeric<size_t> plane_height =
          (base::CheckedNumeric<size_t>(size.height()) + subsample_factor - 1) /
          subsample_factor;
      if (!gfx::RowSizeForBufferFormatChecked(size.width(), format, i,
                                              &min_stride) ||
          handle.planes[i].stride < min_stride) {
        return nullptr;
      }

      base::CheckedNumeric<size_t> min_size =
          base::CheckedNumeric<size_t>(handle.planes[i].stride) * plane_height;
      if (!min_size.IsValid() || handle.planes[i].size < min_size.ValueOrDie())
        return nullptr;

      base::CheckedNumeric<size_t> end_pos =
          base::CheckedNumeric<size_t>(handle.planes[i].offset) +
          handle.planes[i].size;
      if (!end_pos.IsValid() || end_pos.ValueOrDie() > vmo_size)
        return nullptr;
    }

    return std::make_unique<ClientNativePixmapFuchsia>(std::move(handle));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScenicClientNativePixmapFactory);
};

gfx::ClientNativePixmapFactory* CreateClientNativePixmapFactoryScenic() {
  return new ScenicClientNativePixmapFactory();
}

}  // namespace ui
