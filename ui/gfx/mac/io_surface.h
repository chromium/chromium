// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MAC_IO_SURFACE_H_
#define UI_GFX_MAC_IO_SURFACE_H_

#include <IOSurface/IOSurface.h>
#include <mach/mach.h>

#include "base/mac/scoped_cftyperef.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/generic_shared_memory_id.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gfx_export.h"

namespace gfx {

namespace internal {

struct IOSurfaceMachPortTraits {
  GFX_EXPORT static mach_port_t InvalidValue() { return MACH_PORT_NULL; }
  GFX_EXPORT static mach_port_t Retain(mach_port_t);
  GFX_EXPORT static void Release(mach_port_t);
};

struct ScopedInUseIOSurfaceTraits {
  static IOSurfaceRef InvalidValue() { return nullptr; }
  static IOSurfaceRef Retain(IOSurfaceRef io_surface) {
    CFRetain(io_surface);
    IOSurfaceIncrementUseCount(io_surface);
    return io_surface;
  }
  static void Release(IOSurfaceRef io_surface) {
    IOSurfaceDecrementUseCount(io_surface);
    CFRelease(io_surface);
  }
};

}  // namespace internal

using IOSurfaceId = GenericSharedMemoryId;

// Helper function to create an IOSurface with a specified size and format.
// The surface is zero-initialized if |should_clear| is true. This is not
// necessary for anonymous surfaces that are not exported to renderers and used
// as render targets only.
GFX_EXPORT IOSurfaceRef CreateIOSurface(const Size& size,
                                        BufferFormat format,
                                        bool should_clear = true);

// A scoper for handling Mach port names that are send rights for IOSurfaces.
// This scoper is both copyable and assignable, which will increase the kernel
// reference count of the right. On destruction, the reference count is
// decremented.
using ScopedRefCountedIOSurfaceMachPort =
    base::ScopedTypeRef<mach_port_t, internal::IOSurfaceMachPortTraits>;

// A scoper for holding a reference to an IOSurface and also incrementing its
// in-use counter while the scoper exists.
using ScopedInUseIOSurface =
    base::ScopedTypeRef<IOSurfaceRef, internal::ScopedInUseIOSurfaceTraits>;

// Return true if there exists a value for IOSurfaceColorSpace or
// IOSurfaceICCProfile that will make CoreAnimation render using |color_space|.
GFX_EXPORT bool IOSurfaceCanSetColorSpace(const gfx::ColorSpace& color_space);

// Set color space for given IOSurface. IOSurfaceCanSetColorSpace must return
// true for |color_space| otherwise this does nothing.
GFX_EXPORT void IOSurfaceSetColorSpace(IOSurfaceRef io_surface,
                                       const gfx::ColorSpace& color_space);

}  // namespace gfx

#endif  // UI_GFX_MAC_IO_SURFACE_H_
