// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/ozone/platform/flatland/client_native_pixmap_factory_flatland.h"

#include <lib/zx/vmar.h>
#include <lib/zx/vmo.h>

#include <bit>
#include <vector>

#include "base/check_op.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/koid.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/system/sys_info.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/client_native_pixmap.h"
#include "ui/gfx/client_native_pixmap_factory.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace {

class ClientNativePixmapFuchsia final : public gfx::ClientNativePixmap {
 public:
  ~ClientNativePixmapFuchsia() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (mapping_) {
      // Flush the cache if Unmap is not called before the pixmap is destroyed.
      if (logically_mapped_) {
        Unmap();
      }

      zx_status_t status = zx::vmar::root_self()->unmap(
          reinterpret_cast<uintptr_t>(mapping_), mapping_size_);
      ZX_DCHECK(status == ZX_OK, status) << "zx_vmar_unmap";
    }
  }

  ClientNativePixmapFuchsia(const ClientNativePixmapFuchsia&) = delete;
  ClientNativePixmapFuchsia& operator=(const ClientNativePixmapFuchsia&) =
      delete;

  bool Map() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (handle_.planes.empty()) {
      CHECK(!mapping_);
      return false;
    }

    if (mapping_) {
      logically_mapped_ = true;
      return true;
    }

    // When reaching here, we can assume,
    // 1. all the planes are pointing to the same underlying VM objects.
    // 2. the end of last plane should cover all the memory blocks.
    // 3. vmo.get_size() should return a size to cover all the planes.
    // 4. vmo.get_size() should return a size well aligned with ZX_PAGE_SIZE.
    // See checks being performed in the CreateFromHandle.

    CHECK(handle_.planes[0].vmo);
    CHECK_EQ(handle_.planes[0].vmo.get_size(&mapping_size_), ZX_OK);
    CHECK_EQ(mapping_size_ % ZX_PAGE_SIZE, 0UL);

    // Pre-commit the pages of the pixmap, since it is likely that every page
    // will be touched. This is also necessary to successfully pre-fill the page
    // table entries during the subsequent map operation.
    zx_status_t status = handle_.planes[0].vmo.op_range(
        ZX_VMO_OP_COMMIT, 0, mapping_size_, nullptr, 0);
    ZX_DCHECK(status == ZX_OK, status) << "zx_vmo_op_range";

    // ZX_VM_MAP_RANGE pre-fills the page table entries for committed pages to
    // avoid unnecessary page faults.
    zx_vaddr_t addr;
    status = zx::vmar::root_self()->map(
        ZX_VM_PERM_READ | ZX_VM_PERM_WRITE | ZX_VM_MAP_RANGE, 0,
        handle_.planes[0].vmo, 0, mapping_size_, &addr);
    if (status != ZX_OK) {
      ZX_DLOG(ERROR, status) << "zx_vmar_map";
      return false;
    }
    mapping_ = reinterpret_cast<uint8_t*>(addr);
    logically_mapped_ = true;

    return true;
  }

  void Unmap() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(mapping_);
    DCHECK(logically_mapped_);

    // Flush the CPu cache in case the GPU reads the data directly from RAM.
    // Keep the mapping to avoid unnecessary overhead when later reusing the
    // pixmap. The actual unmap happens when the pixmap is destroyed.
    if (handle_.ram_coherency) {
      zx_status_t status =
          zx_cache_flush(mapping_, mapping_size_,
                         ZX_CACHE_FLUSH_DATA | ZX_CACHE_FLUSH_INVALIDATE);
      ZX_DCHECK(status == ZX_OK, status) << "zx_cache_flush";
    }
    logically_mapped_ = false;
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

  static std::unique_ptr<gfx::ClientNativePixmap> CreateFromHandle(
      gfx::NativePixmapHandle handle,
      const gfx::Size& size,
      gfx::BufferFormat format) {
    // |planes| may be empty for non-mappable pixmaps. No need to validate the
    // handle in that case.
    if (handle.planes.empty()) {
      return CreateUniquePtr(std::move(handle));
    }

    // Validate that all planes refer to a single memory object.
    const std::optional<zx_koid_t> first_plane_koid =
        base::GetKoid(handle.planes[0].vmo);
    if (!first_plane_koid) {
      return nullptr;
    }
    for (const auto& plane : handle.planes) {
      const std::optional<zx_koid_t> plane_koid = base::GetKoid(plane.vmo);
      DCHECK(plane.vmo.is_valid() || !plane_koid);
      if (plane_koid != first_plane_koid) {
        return nullptr;
      }
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

    return CreateUniquePtr(std::move(handle));
  }

 private:
  // Allow being created only by the factory method above.
  explicit ClientNativePixmapFuchsia(gfx::NativePixmapHandle handle)
      : handle_(std::move(handle)) {
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  // A shortcut to call private constructor.
  static std::unique_ptr<gfx::ClientNativePixmap> CreateUniquePtr(
      gfx::NativePixmapHandle handle) {
    return base::WrapUnique<gfx::ClientNativePixmap>(
        new ClientNativePixmapFuchsia(std::move(handle)));
  }

  gfx::NativePixmapHandle handle_;

  SEQUENCE_CHECKER(sequence_checker_);
  bool logically_mapped_ = false;
  uint8_t* mapping_ = nullptr;
  size_t mapping_size_ = 0;
};

}  // namespace

namespace ui {

class FlatlandClientNativePixmapFactory final
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
    return ClientNativePixmapFuchsia::CreateFromHandle(std::move(handle), size,
                                                       format);
  }
};

gfx::ClientNativePixmapFactory* CreateClientNativePixmapFactoryFlatland() {
  return new FlatlandClientNativePixmapFactory();
}

}  // namespace ui
