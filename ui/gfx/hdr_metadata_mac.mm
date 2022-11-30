// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/hdr_metadata_mac.h"
#include "ui/gfx/hdr_metadata.h"

#include <simd/simd.h>

namespace gfx {

base::ScopedCFTypeRef<CFDataRef> GenerateContentLightLevelInfo(
    const gfx::HDRMetadata& hdr_metadata) {
  // This is a SMPTEST2086 Content Light Level Information box.
  struct ContentLightLevelInfoSEI {
    uint16_t max_content_light_level;
    uint16_t max_frame_average_light_level;
  } __attribute__((packed, aligned(2)));
  static_assert(sizeof(ContentLightLevelInfoSEI) == 4, "Must be 4 bytes");

  // Values are stored in big-endian...
  ContentLightLevelInfoSEI sei;
  sei.max_content_light_level =
      __builtin_bswap16(hdr_metadata.max_content_light_level);
  sei.max_frame_average_light_level =
      __builtin_bswap16(hdr_metadata.max_frame_average_light_level);

  return base::ScopedCFTypeRef<CFDataRef>(
      CFDataCreate(nullptr, reinterpret_cast<const UInt8*>(&sei), 4));
}

base::ScopedCFTypeRef<CFDataRef> GenerateMasteringDisplayColorVolume(
    const gfx::HDRMetadata& hdr_metadata) {
  // This is a SMPTEST2086 Mastering Display Color Volume box.
  struct MasteringDisplayColorVolumeSEI {
    vector_ushort2 primaries[3];  // GBR
    vector_ushort2 white_point;
    uint32_t luminance_max;
    uint32_t luminance_min;
  } __attribute__((packed, aligned(4)));
  static_assert(sizeof(MasteringDisplayColorVolumeSEI) == 24,
                "Must be 24 bytes");

  // Make a copy which we can manipulate.
  auto md = hdr_metadata.color_volume_metadata;

  constexpr float kColorCoordinateUpperBound = 50000.0f;
  md.primary_r.Scale(kColorCoordinateUpperBound);
  md.primary_g.Scale(kColorCoordinateUpperBound);
  md.primary_b.Scale(kColorCoordinateUpperBound);
  md.white_point.Scale(kColorCoordinateUpperBound);

  constexpr float kUnitOfMasteringLuminance = 10000.0f;
  md.luminance_max *= kUnitOfMasteringLuminance;
  md.luminance_min *= kUnitOfMasteringLuminance;

  // Values are stored in big-endian...
  MasteringDisplayColorVolumeSEI sei;
  sei.primaries[0].x = __builtin_bswap16(md.primary_g.x() + 0.5f);
  sei.primaries[0].y = __builtin_bswap16(md.primary_g.y() + 0.5f);
  sei.primaries[1].x = __builtin_bswap16(md.primary_b.x() + 0.5f);
  sei.primaries[1].y = __builtin_bswap16(md.primary_b.y() + 0.5f);
  sei.primaries[2].x = __builtin_bswap16(md.primary_r.x() + 0.5f);
  sei.primaries[2].y = __builtin_bswap16(md.primary_r.y() + 0.5f);
  sei.white_point.x = __builtin_bswap16(md.white_point.x() + 0.5f);
  sei.white_point.y = __builtin_bswap16(md.white_point.y() + 0.5f);
  sei.luminance_max = __builtin_bswap32(md.luminance_max + 0.5f);
  sei.luminance_min = __builtin_bswap32(md.luminance_min + 0.5f);

  return base::ScopedCFTypeRef<CFDataRef>(
      CFDataCreate(nullptr, reinterpret_cast<const UInt8*>(&sei), 24));
}

}  // namespace gfx
