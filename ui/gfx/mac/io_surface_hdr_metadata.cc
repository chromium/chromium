// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/mac/io_surface_hdr_metadata.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "ui/gfx/mojom/hdr_metadata.mojom.h"

namespace gfx {

namespace {

// The key under which HDR metadata is attached to an IOSurface.
const CFStringRef kCrIOSurfaceHDRMetadataKey =
    CFSTR("CrIOSurfaceHDRMetadataKey");

}  // namespace

void IOSurfaceSetHDRMetadata(IOSurfaceRef io_surface,
                             gfx::HDRMetadata hdr_metadata) {
  std::vector<uint8_t> std_data =
      gfx::mojom::HDRMetadata::Serialize(&hdr_metadata);
  base::ScopedCFTypeRef<CFDataRef> cf_data(
      CFDataCreate(nullptr, std_data.data(), std_data.size()));
  IOSurfaceSetValue(io_surface, kCrIOSurfaceHDRMetadataKey, cf_data);
}

bool IOSurfaceGetHDRMetadata(IOSurfaceRef io_surface,
                             gfx::HDRMetadata& hdr_metadata) {
  base::ScopedCFTypeRef<CFTypeRef> cf_untyped(
      IOSurfaceCopyValue(io_surface, kCrIOSurfaceHDRMetadataKey));
  CFDataRef cf_data = base::mac::CFCast<CFDataRef>(cf_untyped);
  if (!cf_data)
    return false;
  const UInt8* raw_data = CFDataGetBytePtr(cf_data);
  std::vector<uint8_t> std_data(raw_data, raw_data + CFDataGetLength(cf_data));
  return gfx::mojom::HDRMetadata::Deserialize(std_data, &hdr_metadata);
}

}  // namespace gfx
