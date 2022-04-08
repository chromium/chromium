// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/native_pixmap_handle.h"

#include <utility>

#include "base/logging.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <drm_fourcc.h>
#include <unistd.h>

#include "base/posix/eintr_wrapper.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/zx/vmo.h>
#include "base/fuchsia/fuchsia_logging.h"
#endif

namespace gfx {

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
static_assert(NativePixmapHandle::kNoModifier == DRM_FORMAT_MOD_INVALID,
              "gfx::NativePixmapHandle::kNoModifier should be an alias for"
              "DRM_FORMAT_MOD_INVALID");
#endif

NativePixmapPlane::NativePixmapPlane() : stride(0), offset(0), size(0) {}

NativePixmapPlane::NativePixmapPlane(int stride,
                                     int offset,
                                     uint64_t size
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
                                     ,
                                     base::ScopedFD fd
#elif BUILDFLAG(IS_FUCHSIA)
                                     ,
                                     zx::vmo vmo
#endif
                                     )
    : stride(stride),
      offset(offset),
      size(size)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
      ,
      fd(std::move(fd))
#elif BUILDFLAG(IS_FUCHSIA)
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
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
    DCHECK(plane.fd.is_valid());
    // Combining the HANDLE_EINTR and ScopedFD's constructor causes the compiler
    // to emit some very strange assembly that tends to cause FD ownership
    // violations. see crbug.com/c/1287325.
    int checked_dup = HANDLE_EINTR(dup(plane.fd.get()));
    base::ScopedFD fd_dup(checked_dup);
    if (!fd_dup.is_valid()) {
      PLOG(ERROR) << "dup";
      return NativePixmapHandle();
    }
    clone.planes.emplace_back(plane.stride, plane.offset, plane.size,
                              std::move(fd_dup));
#elif BUILDFLAG(IS_FUCHSIA)
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

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  clone.modifier = handle.modifier;
  clone.supports_zero_copy_webgpu_import =
      handle.supports_zero_copy_webgpu_import;
#endif

#if BUILDFLAG(IS_FUCHSIA)
  clone.buffer_collection_id = handle.buffer_collection_id;
  clone.buffer_index = handle.buffer_index;
  clone.ram_coherency = handle.ram_coherency;
#endif

  return clone;
}

}  // namespace gfx
