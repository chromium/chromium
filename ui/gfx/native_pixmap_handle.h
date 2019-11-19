// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_NATIVE_PIXMAP_HANDLE_H_
#define UI_GFX_NATIVE_PIXMAP_HANDLE_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/optional.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "ui/gfx/gfx_export.h"

#if defined(OS_LINUX)
#include "base/files/scoped_file.h"
#endif

#if defined(OS_FUCHSIA)
#include <lib/zx/vmo.h>
#endif

namespace gfx {

// NativePixmapPlane is used to carry the plane related information for GBM
// buffer. More fields can be added if they are plane specific.
struct GFX_EXPORT NativePixmapPlane {
  NativePixmapPlane();
  NativePixmapPlane(int stride,
                    int offset,
                    uint64_t size
#if defined(OS_LINUX)
                    ,
                    base::ScopedFD fd
#elif defined(OS_FUCHSIA)
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

#if defined(OS_LINUX)
  // File descriptor for the underlying memory object (usually dmabuf).
  base::ScopedFD fd;
#elif defined(OS_FUCHSIA)
  zx::vmo vmo;
#endif
};

#if defined(OS_FUCHSIA)
// Buffer collection ID is used to identify sysmem buffer collections across
// processes.
using SysmemBufferCollectionId = base::UnguessableToken;
#endif

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

#if defined(OS_LINUX)
  // The modifier is retrieved from GBM library and passed to EGL driver.
  // Generally it's platform specific, and we don't need to modify it in
  // Chromium code. Also one per plane per entry.
  uint64_t modifier = kNoModifier;
#endif

#if defined(OS_FUCHSIA)
  base::Optional<SysmemBufferCollectionId> buffer_collection_id;
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

}  // namespace gfx

#endif  // UI_GFX_NATIVE_PIXMAP_HANDLE_H_
