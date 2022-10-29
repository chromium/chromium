// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/flatland/client_native_pixmap_factory_flatland.h"

#include <lib/zx/vmar.h>
#include <lib/zx/vmo.h>
#include <vector>

#include "base/bits.h"
#include "base/check_op.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/koid.h"
#include "base/numerics/safe_conversions.h"
#include "base/system/sys_info.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/client_native_pixmap.h"
#include "ui/gfx/client_native_pixmap_factory.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace ui {

namespace {

bool AlignUpToPageSizeChecked(size_t size, size_t* aligned_size) {
  static_assert(base::IsValueInRangeForNumericType<size_t>(ZX_PAGE_SIZE) &&
                    base::bits::IsPowerOfTwo(ZX_PAGE_SIZE),
                "The page size must fit in a size_t and be a power of 2.");
  constexpr size_t kPageSizeMinusOne = ZX_PAGE_SIZE - 1;
  base::CheckedNumeric<size_t> aligned_size_checked =
      base::CheckAdd(size, kPageSizeMinusOne) & (~kPageSizeMinusOne);
  if (!aligned_size_checked.IsValid())
    return false;
  *aligned_size = aligned_size_checked.ValueOrDie();
  return true;
}

}  // namespace

class ClientNativePixmapFuchsia : public gfx::ClientNativePixmap {
 public:
  explicit ClientNativePixmapFuchsia(gfx::NativePixmapHandle handle)
      : handle_(std::move(handle)) {}

  ~ClientNativePixmapFuchsia() override {
    if (mapping_)
      Unmap();
  }

  ClientNativePixmapFuchsia(const ClientNativePixmapFuchsia&) = delete;
  ClientNativePixmapFuchsia& operator=(const ClientNativePixmapFuchsia&) =
      delete;

  bool Map() override {
    if (mapping_)
      return true;

    if (handle_.planes.empty() || !handle_.planes[0].vmo)
      return false;

    // Assume that the last plane is at the end of the VMO. If this assumption
    // is violated, we shouldn't get here because
    // FlatlandClientNativePixmapFactory::ImportFromHandle() validates (through
    // CanFitImageForSizeAndFormat()) that the (offset + size) for each plane is
    // less than or equal to |last_plane_end|.
    //
    // Note: the |last_plane_end| computation has been determined to not
    // overflow in FlatlandClientNativePixmapFactory::ImportFromHandle()
    // (through CanFitImageForSizeAndFormat()).
    const size_t last_plane_end =
        base::CheckAdd(handle_.planes.back().offset, handle_.planes.back().size)
            .ValueOrDie<size_t>();

#if DCHECK_IS_ON()
    // All planes should fall within the range that ends with the last plane.
    // This has been verified by
    // FlatlandClientNativePixmapFactory::ImportFromHandle() (through
    // CanFitImageForSizeAndFormat()).
    for (auto& plane : handle_.planes) {
      DCHECK(base::CheckAdd(plane.offset, plane.size).ValueOrDie<size_t>() <=
             last_plane_end);
    }
#endif

    // Round mapping size to align with the page size. Mapping
    // |aligned_mapping_size| bytes should be safe because
    // FlatlandClientNativePixmapFactory::ImportFromHandle() ensures that
    // |last_plane_end| <= <size of the VMO> where <size of the VMO> is
    // page_aligned which implies that |aligned_mapping_size| <= <size of the
    // VMO>.
    size_t aligned_mapping_size;
    if (!AlignUpToPageSizeChecked(last_plane_end, &aligned_mapping_size))
      return false;
    mapping_size_ = aligned_mapping_size;

    zx_vaddr_t addr;
    zx_status_t status = zx::vmar::root_self()->map(
        ZX_VM_PERM_READ | ZX_VM_PERM_WRITE, 0, handle_.planes[0].vmo, 0,
        mapping_size_, &addr);
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

  size_t GetNumberOfPlanes() const override { return handle_.planes.size(); }

  void* GetMemoryAddress(size_t plane) const override {
    DCHECK_LT(plane, handle_.planes.size());
    DCHECK(mapping_);
    return mapping_ + handle_.planes[plane].offset;
  }

  int GetStride(size_t plane) const override {
    DCHECK_LT(plane, handle_.planes.size());
    return base::checked_cast<int>(handle_.planes[plane].stride);
  }

  gfx::NativePixmapHandle CloneHandleForIPC() const override {
    return gfx::CloneHandleForIPC(handle_);
  }

 private:
  gfx::NativePixmapHandle handle_;

  uint8_t* mapping_ = nullptr;
  size_t mapping_size_ = 0;
};

class FlatlandClientNativePixmapFactory
    : public gfx::ClientNativePixmapFactory {
 public:
  FlatlandClientNativePixmapFactory() = default;
  ~FlatlandClientNativePixmapFactory() override = default;
  FlatlandClientNativePixmapFactory(const FlatlandClientNativePixmapFactory&) =
      delete;
  FlatlandClientNativePixmapFactory& operator=(
      const FlatlandClientNativePixmapFactory&) = delete;

  std::unique_ptr<gfx::ClientNativePixmap> ImportFromHandle(
      gfx::NativePixmapHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format,
      gfx::BufferUsage usage) override {
    // |planes| may be empty for non-mappable pixmaps. No need to validate the
    // handle in that case.
    if (handle.planes.empty())
      return std::make_unique<ClientNativePixmapFuchsia>(std::move(handle));

    // Validate that all planes refer to a single memory object.
    const absl::optional<zx_koid_t> first_plane_koid =
        base::GetKoid(handle.planes[0].vmo);
    if (!first_plane_koid)
      return nullptr;
    for (const auto& plane : handle.planes) {
      const absl::optional<zx_koid_t> plane_koid = base::GetKoid(plane.vmo);
      DCHECK(plane.vmo.is_valid() || !plane_koid);
      if (plane_koid != first_plane_koid)
        return nullptr;
    }

    if (!CanFitImageForSizeAndFormat(handle, size, format,
                                     /*assume_single_memory_object=*/true)) {
      return nullptr;
    }

    // The |last_plane_end| computation should not overflow if the
    // CanFitImageForSizeAndFormat() check passed.
    const size_t last_plane_end =
        base::CheckAdd(handle.planes.back().offset, handle.planes.back().size)
            .ValueOrDie<size_t>();

    uint64_t vmo_size;
    if (handle.planes.back().vmo.get_size(&vmo_size) != ZX_OK ||
        !base::IsValueInRangeForNumericType<size_t>(vmo_size) ||
        base::checked_cast<size_t>(vmo_size) < last_plane_end) {
      return nullptr;
    }

#if DCHECK_IS_ON()
    // zx_vmo_get_size() should return a page-aligned size. This is important
    // because we request a page-aligned size in
    // ClientNativePixmapFuchsia::Map().
    DCHECK_EQ(vmo_size % ZX_PAGE_SIZE, 0u);

    // The CanFitImageForSizeAndFormat() call above should guarantee that the
    // (offset + size) for each plane is <= |last_plane_end|, and since we now
    // know that |last_plane_end| <= |vmo_size|, then (offset + size) for each
    // plane <= |vmo_size|.
    for (const auto& plane : handle.planes) {
      DCHECK_LE(plane.offset + plane.size, vmo_size);
    }
#endif

    return std::make_unique<ClientNativePixmapFuchsia>(std::move(handle));
  }
};

gfx::ClientNativePixmapFactory* CreateClientNativePixmapFactoryFlatland() {
  return new FlatlandClientNativePixmapFactory();
}

}  // namespace ui
