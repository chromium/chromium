// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_HDR_METADATA_MAC_H_
#define UI_GFX_HDR_METADATA_MAC_H_

#include <CoreFoundation/CoreFoundation.h>

#include <optional>

#include "base/apple/scoped_cftyperef.h"
#include "ui/gfx/color_space_export.h"

namespace gfx {

struct HDRMetadata;

// This can be used for rendering content using AVSampleBufferDisplayLayer via
// the key kCVImageBufferContentLightLevelInfoKey or for rendering content using
// a CAMetalLayer via CAEDRMetadata.
COLOR_SPACE_EXPORT base::apple::ScopedCFTypeRef<CFDataRef>
GenerateContentLightLevelInfo(
    const std::optional<gfx::HDRMetadata>& hdr_metadata);

// This can be used for rendering content using AVSampleBufferDisplayLayer via
// the key kCVImageBufferMasteringDisplayColorVolumeKey or for rendering content
// using a CAMetalLayer via CAEDRMetadata.
COLOR_SPACE_EXPORT base::apple::ScopedCFTypeRef<CFDataRef>
GenerateMasteringDisplayColorVolume(
    const std::optional<gfx::HDRMetadata>& hdr_metadata);

}  // namespace gfx

#endif  // UI_GFX_HDR_METADATA_MAC_H_
