// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/native_pixmap_handle.h"

#include <utility>

#include "build/build_config.h"

#if defined(OS_LINUX)
#include <drm_fourcc.h>
#include "base/posix/eintr_wrapper.h"
#endif

#if defined(OS_FUCHSIA)
#include <lib/zx/vmo.h>
#include "base/fuchsia/fuchsia_logging.h"
#endif

namespace gfx {

#if defined(OS_LINUX)
static_assert(NativePixmapHandle::kNoModifier == DRM_FORMAT_MOD_INVALID,
              "gfx::NativePixmapHandle::kNoModifier should be an alias for"
              "DRM_FORMAT_MOD_INVALID");
#endif

NativePixmapPlane::NativePixmapPlane() : stride(0), offset(0), size(0) {}

NativePixmapPlane::NativePixmapPlane(int stride,
                                     int offset,
                                     uint64_t size
#if defined(OS_LINUX)
                                     ,
                                     base::ScopedFD fd
#elif defined(OS_FUCHSIA)
                                     ,
                                     zx::vmo vmo
#endif
                                     )
    : stride(stride),
      offset(offset),
      size(size)
#if defined(OS_LINUX)
      ,
      fd(std::move(fd))
#elif defined(OS_FUCHSIA)
      ,
      vmo(std::move(vmo))
#endif
{
}

NativePixmapPlane::NativePixmapPlane(NativePixmapPlane&& other) = default;

NativePixmapPlane::~NativePixmapPlane() = default;

NativePixmapPlane& NativePixmapPlane::operator=(NativePixmapPlane&& other) =
    default;

NativePixmapHandle::NativePixmapHandle() = default;
NativePixmapHandle::NativePixmapHandle(NativePixmapHandle&& other) = default;

NativePixmapHandle::~NativePixmapHandle() = default;

NativePixmapHandle& NativePixmapHandle::operator=(NativePixmapHandle&& other) =
    default;

NativePixmapHandle CloneHandleForIPC(const NativePixmapHandle& handle) {
  NativePixmapHandle clone;
  for (auto& plane : handle.planes) {
#if defined(OS_LINUX)
    DCHECK(plane.fd.is_valid());
    base::ScopedFD fd_dup(HANDLE_EINTR(dup(plane.fd.get())));
    if (!fd_dup.is_valid()) {
      PLOG(ERROR) << "dup";
      return NativePixmapHandle();
    }
    clone.planes.emplace_back(plane.stride, plane.offset, plane.size,
                              std::move(fd_dup));
#elif defined(OS_FUCHSIA)
    zx::vmo vmo_dup;
    // VMO may be set to NULL for pixmaps that cannot be mapped.
    if (plane.vmo) {
      zx_status_t status = plane.vmo.duplicate(ZX_RIGHT_SAME_RIGHTS, &vmo_dup);
      if (status != ZX_OK) {
        ZX_DLOG(ERROR, status) << "zx_handle_duplicate";
        return NativePixmapHandle();
      }
    }
    clone.planes.emplace_back(plane.stride, plane.offset, plane.size,
                              std::move(vmo_dup));
#else
#error Unsupported OS
#endif
  }

#if defined(OS_LINUX)
  clone.modifier = handle.modifier;
#endif

#if defined(OS_FUCHSIA)
  clone.buffer_collection_id = handle.buffer_collection_id;
  clone.buffer_index = handle.buffer_index;
  clone.ram_coherency = handle.ram_coherency;
#endif

  return clone;
}

}  // namespace gfx
