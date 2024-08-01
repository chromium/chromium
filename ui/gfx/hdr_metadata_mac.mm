// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/354829279): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/gfx/hdr_metadata_mac.h"
#include "ui/gfx/hdr_metadata.h"

#include <simd/simd.h>

namespace gfx {

base::apple::ScopedCFTypeRef<CFDataRef> GenerateContentLightLevelInfo(
    const std::optional<gfx::HDRMetadata>& hdr_metadata) {
  if (!hdr_metadata || !hdr_metadata->cta_861_3 ||
      hdr_metadata->cta_861_3->max_content_light_level == 0.f ||
      hdr_metadata->cta_861_3->max_frame_average_light_level == 0.f) {
    return base::apple::ScopedCFTypeRef<CFDataRef>();
  }

  // This is a SMPTEST2086 Content Light Level Information box.
  struct ContentLightLevelInfoSEI {
    uint16_t max_content_light_level;
    uint16_t max_frame_average_light_level;
  } __attribute__((packed, aligned(2)));
  static_assert(sizeof(ContentLightLevelInfoSEI) == 4, "Must be 4 bytes");

  // Values are stored in big-endian...
  ContentLightLevelInfoSEI sei;
  sei.max_content_light_level =
      __builtin_bswap16(hdr_metadata->cta_861_3->max_content_light_level);
  sei.max_frame_average_light_level =
      __builtin_bswap16(hdr_metadata->cta_861_3->max_frame_average_light_level);

  return base::apple::ScopedCFTypeRef<CFDataRef>(
      CFDataCreate(nullptr, reinterpret_cast<const UInt8*>(&sei), 4));
}

base::apple::ScopedCFTypeRef<CFDataRef> GenerateMasteringDisplayColorVolume(
    const std::optional<gfx::HDRMetadata>& hdr_metadata) {
  // This is a SMPTEST2086 Mastering Display Color Volume box.
  struct MasteringDisplayColorVolumeSEI {
    vector_ushort2 primaries[3];  // GBR
    vector_ushort2 white_point;
    uint32_t luminance_max;
    uint32_t luminance_min;
  } __attribute__((packed, aligned(4)));
  static_assert(sizeof(MasteringDisplayColorVolumeSEI) == 24,
                "Must be 24 bytes");

  // Make a copy with all values populated, and which we can manipulate.
  auto md =
      HDRMetadata::PopulateUnspecifiedWithDefaults(hdr_metadata).smpte_st_2086;

  constexpr float kColorCoordinateUpperBound = 50000.0f;
  constexpr float kUnitOfMasteringLuminance = 10000.0f;
  md->luminance_max *= kUnitOfMasteringLuminance;
  md->luminance_min *= kUnitOfMasteringLuminance;

  // Values are stored in big-endian...
  MasteringDisplayColorVolumeSEI sei;
  const auto& primaries = md->primaries;
  sei.primaries[0].x =
      __builtin_bswap16(primaries.fGX * kColorCoordinateUpperBound + 0.5f);
  sei.primaries[0].y =
      __builtin_bswap16(primaries.fGY * kColorCoordinateUpperBound + 0.5f);
  sei.primaries[1].x =
      __builtin_bswap16(primaries.fBX * kColorCoordinateUpperBound + 0.5f);
  sei.primaries[1].y =
      __builtin_bswap16(primaries.fBY * kColorCoordinateUpperBound + 0.5f);
  sei.primaries[2].x =
      __builtin_bswap16(primaries.fRX * kColorCoordinateUpperBound + 0.5f);
  sei.primaries[2].y =
      __builtin_bswap16(primaries.fRY * kColorCoordinateUpperBound + 0.5f);
  sei.white_point.x =
      __builtin_bswap16(primaries.fWX * kColorCoordinateUpperBound + 0.5f);
  sei.white_point.y =
      __builtin_bswap16(primaries.fWY * kColorCoordinateUpperBound + 0.5f);
  sei.luminance_max = __builtin_bswap32(md->luminance_max + 0.5f);
  sei.luminance_min = __builtin_bswap32(md->luminance_min + 0.5f);

  return base::apple::ScopedCFTypeRef<CFDataRef>(
      CFDataCreate(nullptr, reinterpret_cast<const UInt8*>(&sei), 24));
}

}  // namespace gfx
