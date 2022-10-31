// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_NATIVE_PIXMAP_HANDLE_H_
#define UI_GFX_NATIVE_PIXMAP_HANDLE_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "build/build_config.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/gfx_export.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include "base/files/scoped_file.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include <lib/zx/eventpair.h>
#include <lib/zx/vmo.h>
#endif

namespace gfx {

class Size;

// NativePixmapPlane is used to carry the plane related information for GBM
// buffer. More fields can be added if they are plane specific.
struct GFX_EXPORT NativePixmapPlane {
  NativePixmapPlane();
  NativePixmapPlane(int stride,
                    int offset,
                    uint64_t size
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
                    ,
                    base::ScopedFD fd
#elif BUILDFLAG(IS_FUCHSIA)
                    ,
                    zx::vmo vmo
#endif
  );
  NativePixmapPlane(NativePixmapPlane&& other);
  ~NativePixmapPlane();

  NativePixmapPlane& operator=(NativePixmapPlane&& other);

  // The strides and offsets in bytes to be used when accessing the buffers via
  // a memory mapping. One per plane per entry.
  uint32_t stride;
  uint64_t offset;
  // Size in bytes of the plane.
  // This is necessary to map the buffers.
  uint64_t size;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // File descriptor for the underlying memory object (usually dmabuf).
  base::ScopedFD fd;
#elif BUILDFLAG(IS_FUCHSIA)
  zx::vmo vmo;
#endif
};

struct GFX_EXPORT NativePixmapHandle {
  // This is the same value as DRM_FORMAT_MOD_INVALID, which is not a valid
  // modifier. We use this to indicate that layout information
  // (tiling/compression) if any will be communicated out of band.
  static constexpr uint64_t kNoModifier = 0x00ffffffffffffffULL;

  NativePixmapHandle();
  NativePixmapHandle(NativePixmapHandle&& other);

  ~NativePixmapHandle();

  NativePixmapHandle& operator=(NativePixmapHandle&& other);

  std::vector<NativePixmapPlane> planes;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  // The modifier is retrieved from GBM library and passed to EGL driver.
  // Generally it's platform specific, and we don't need to modify it in
  // Chromium code. Also one per plane per entry.
  uint64_t modifier = kNoModifier;

  // WebGPU can directly import the handle to create texture from it.
  bool supports_zero_copy_webgpu_import = false;
#endif

#if BUILDFLAG(IS_FUCHSIA)
  // Sysmem buffer collection handle. The other end of the eventpair is owned
  // by the SysmemBufferCollection instance in the GPU process. It will destroy
  // itself when all handles for the collection are dropped. Eventpair is used
  // here because they are dupable, nun-fungible and unique.
  zx::eventpair buffer_collection_handle;
  uint32_t buffer_index = 0;

  // Set to true for sysmem buffers which are initialized with RAM coherency
  // domain. This means that clients that write to the buffers must flush CPU
  // cache.
  bool ram_coherency = false;
#endif
};

// Returns an instance of |handle| which can be sent over IPC. This duplicates
// the file-handles, so that the IPC code take ownership of them, without
// invalidating |handle|.
GFX_EXPORT NativePixmapHandle
CloneHandleForIPC(const NativePixmapHandle& handle);

// Returns true iff the plane metadata (number of planes, plane size, offset,
// and stride) in |handle| corresponds to a buffer that can store an image of
// |size| and |format|. This function does not check the plane handles, so even
// if this function returns true, it's not guaranteed that the memory objects
// referenced by |handle| are consistent with the plane metadata. If
// |assume_single_memory_object| is true, this function assumes that all planes
// in |handle| reference the same memory object and that all planes are
// contained in the range [0, last plane's offset + last plane's size) (and the
// plane metadata is validated against this assumption).
//
// If this function returns true, the caller may make the following additional
// assumptions:
//
// - The stride of each plane can fit in an int (and also in a size_t).
// - If |assume_single_memory_object| is true:
//   - The offset and size of each plane can fit in a size_t.
//   - The result of offset + size for each plane does not overflow and can fit
//     in a size_t.
GFX_EXPORT bool CanFitImageForSizeAndFormat(
    const gfx::NativePixmapHandle& handle,
    const gfx::Size& size,
    gfx::BufferFormat format,
    bool assume_single_memory_object);
}  // namespace gfx

#endif  // UI_GFX_NATIVE_PIXMAP_HANDLE_H_
