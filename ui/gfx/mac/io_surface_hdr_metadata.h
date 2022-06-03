// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_MAC_IO_SURFACE_HDR_METADATA_H_
#define UI_GFX_MAC_IO_SURFACE_HDR_METADATA_H_

#include <IOSurface/IOSurface.h>

#include "base/component_export.h"

namespace gfx {

struct HDRMetadata;

// Attach |hdr_metadata| to |io_surface|. After this is called, any other
// process that has opened |io_surface| will be able to read |hdr_metadata|
// using the function IOSurfaceGetHDRMetadata.
void COMPONENT_EXPORT(GFX_IO_SURFACE_HDR_METADATA)
    IOSurfaceSetHDRMetadata(IOSurfaceRef io_surface,
                            gfx::HDRMetadata hdr_metadata);

// Retrieve in |hdr_metadata| the value that was attached to |io_surface|. This
// will return false on failure.
bool COMPONENT_EXPORT(GFX_IO_SURFACE_HDR_METADATA)
    IOSurfaceGetHDRMetadata(IOSurfaceRef io_surface,
                            gfx::HDRMetadata& hdr_metadata);

}  // namespace gfx

#endif  // UI_GFX_MAC_IO_SURFACE_HDR_METADATA_H_
