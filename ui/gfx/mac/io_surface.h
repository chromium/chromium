// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MAC_IO_SURFACE_H_
#define UI_GFX_MAC_IO_SURFACE_H_

#include <IOKit/IOReturn.h>
#include <IOSurface/IOSurfaceRef.h>
#include <mach/mach.h>

#include "base/apple/scoped_cftyperef.h"
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
// as render targets only. If |override_rgba_to_bgra| is true (default) a BGRA
// IOSurface is created for RGBA/X_8888 BufferFormat. This is needed for GL
// usage since neither ANGLE Metal nor CGL support importing RGBA IOSurfaces,
// whereas for non-GL backends (Dawn and Metal) we want the formats to match.
// TODO(sunnyps): Revisit this when we switch to ANGLE Metal completely since
// wrapping RGBA_8888 can be implemented with Metal quite easily.
GFX_EXPORT base::apple::ScopedCFTypeRef<IOSurfaceRef> CreateIOSurface(
    const Size& size,
    BufferFormat format,
    bool should_clear = true,
#if BUILDFLAG(IS_IOS)
    bool override_rgba_to_bgra = false
#else
    bool override_rgba_to_bgra = true
#endif
);

// A scoper for handling Mach port names that are send rights for IOSurfaces.
// This scoper is both copyable and assignable, which will increase the kernel
// reference count of the right. On destruction, the reference count is
// decremented.
using ScopedRefCountedIOSurfaceMachPort =
    base::apple::ScopedTypeRef<mach_port_t, internal::IOSurfaceMachPortTraits>;

// A scoper for holding a reference to an IOSurface and also incrementing its
// in-use counter while the scoper exists.
using ScopedInUseIOSurface =
    base::apple::ScopedTypeRef<IOSurfaceRef,
                               internal::ScopedInUseIOSurfaceTraits>;

// A scoper for holding a reference to an IOSurface.
using ScopedIOSurface = base::apple::ScopedCFTypeRef<IOSurfaceRef>;

// Return true if there exists a value for IOSurfaceColorSpace or
// IOSurfaceICCProfile that will make CoreAnimation render using |color_space|.
GFX_EXPORT bool IOSurfaceCanSetColorSpace(const gfx::ColorSpace& color_space);

// Set color space for given IOSurface. IOSurfaceCanSetColorSpace must return
// true for |color_space| otherwise this does nothing.
GFX_EXPORT void IOSurfaceSetColorSpace(IOSurfaceRef io_surface,
                                       const gfx::ColorSpace& color_space);

// Return the expected four character code pixel format for an IOSurface with
// the specified gfx::BufferFormat.
GFX_EXPORT uint32_t
BufferFormatToIOSurfacePixelFormat(gfx::BufferFormat format,
                                   bool override_rgba_to_bgra = true);

// Return an IOSurface consuming |io_surface_mach_port|.
GFX_EXPORT base::apple::ScopedCFTypeRef<IOSurfaceRef>
IOSurfaceMachPortToIOSurface(
    ScopedRefCountedIOSurfaceMachPort io_surface_mach_port);

}  // namespace gfx

#endif  // UI_GFX_MAC_IO_SURFACE_H_
