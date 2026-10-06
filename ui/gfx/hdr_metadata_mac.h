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
// a CAMetalLayer via CAEDRMetadata. Returns null if `hdr_metadata` has
// unspecified or invalid CLLI metadata.
COLOR_SPACE_EXPORT base::apple::ScopedCFTypeRef<CFDataRef>
GenerateContentLightLevelInfo(const gfx::HDRMetadata& hdr_metadata);

// This can be used for rendering content using AVSampleBufferDisplayLayer via
// the key kCVImageBufferMasteringDisplayColorVolumeKey or for rendering content
// using a CAMetalLayer via CAEDRMetadata.
//
// If `fallback_to_defaults` is true and `hdr_metadata` has unspecified or
// invalid MDCV metadata, then the result will be populated using the defaults
// from HDRMetadata::PopulateUnspecifiedWithDefaults. If `fallback_to_defaults`
// is false, then null is returned in that case. Note that partially specified
// MDCV metadata is always populated with defaults for its unspecified fields.
COLOR_SPACE_EXPORT base::apple::ScopedCFTypeRef<CFDataRef>
GenerateMasteringDisplayColorVolume(const gfx::HDRMetadata& hdr_metadata,
                                    bool fallback_to_defaults);

// This can be used for rendering content using AVSampleBufferDisplayLayer via
// the key "AmbientViewingEnvironment" or for rendering content using a
// CAMetalLayer via +[CAEDRMetadata HLGMetadataWithAmbientViewingEnvironment].
COLOR_SPACE_EXPORT base::apple::ScopedCFTypeRef<CFDataRef>
GenerateAmbientViewingEnvironment();

}  // namespace gfx

#endif  // UI_GFX_HDR_METADATA_MAC_H_
