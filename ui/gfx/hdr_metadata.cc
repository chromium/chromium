// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/hdr_metadata.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

#include "skia/ext/skcolorspace_primaries.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkData.h"
#include "ui/gfx/switches.h"

namespace gfx {

std::string HdrMetadataCta861_3::ToString() const {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(4)
     << "{maxCLL:" << max_content_light_level
     << ", maxFALL:" << max_frame_average_light_level << "}";
  return ss.str();
}

HdrMetadataSmpteSt2086& HdrMetadataSmpteSt2086::operator=(
    const HdrMetadataSmpteSt2086& rhs) = default;

std::string HdrMetadataSmpteSt2086::ToString() const {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(4) << "{"
     << "red:[" << primaries.fRX << ", " << primaries.fRY << "], "
     << "green:[" << primaries.fGX << ", " << primaries.fGY << "], "
     << "blue:[" << primaries.fBX << ", " << primaries.fBY << "], "
     << "whitePoint:[" << primaries.fWX << ", " << primaries.fWY << "], "
     << "minLum:" << luminance_min << ", "
     << "maxLum:" << luminance_max << "}";
  return ss.str();
}

std::partial_ordering HdrMetadataSmpteSt2086::operator<=>(
    const HdrMetadataSmpteSt2086& rhs) const {
  return std::tie(primaries.fRX, primaries.fRY, primaries.fGX, primaries.fGY,
                  primaries.fBX, primaries.fBY, primaries.fWX, primaries.fWY,
                  luminance_min, luminance_max) <=>
         std::tie(rhs.primaries.fRX, rhs.primaries.fRY, rhs.primaries.fGX,
                  rhs.primaries.fGY, rhs.primaries.fBX, rhs.primaries.fBY,
                  rhs.primaries.fWX, rhs.primaries.fWY, rhs.luminance_min,
                  rhs.luminance_max);
}

std::string HdrMetadataNdwl::ToString() const {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(4) << "{"
     << "nits:" << nits << "}";
  return ss.str();
}

std::string HdrMetadataExtendedRange::ToString() const {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(4) << "{"
     << "current_headroom:" << current_headroom << ", "
     << "desired_headroom:" << desired_headroom << "}";
  return ss.str();
}

HdrMetadataAgtm::HdrMetadataAgtm() = default;

HdrMetadataAgtm::HdrMetadataAgtm(const void* payload, size_t size)
    : payload(SkData::MakeWithCopy(payload, size)) {}

HdrMetadataAgtm::HdrMetadataAgtm(sk_sp<SkData> payload)
    : payload(std::move(payload)) {}

HdrMetadataAgtm::HdrMetadataAgtm(const HdrMetadataAgtm& other) = default;
HdrMetadataAgtm& HdrMetadataAgtm::operator=(const HdrMetadataAgtm& other) =
    default;

HdrMetadataAgtm::~HdrMetadataAgtm() = default;

// static
bool HdrMetadataAgtm::IsEnabled() {
  static bool result = base::FeatureList::IsEnabled(features::kHdrAgtm);
  return result;
}

std::string HdrMetadataAgtm::ToString() const {
  return "agtm placeholder";
}

bool HdrMetadataAgtm::operator==(const HdrMetadataAgtm& rhs) const {
  if (!payload) {
    return !rhs.payload;
  }
  return payload->equals(rhs.payload.get());
}

std::strong_ordering HdrMetadataAgtm::operator<=>(
    const HdrMetadataAgtm& rhs) const {
  if (!payload && !rhs.payload) {
    return std::strong_ordering::equal;
  }
  if (!payload) {
    return std::strong_ordering::less;
  }
  if (!rhs.payload) {
    return std::strong_ordering::greater;
  }
  return std::lexicographical_compare_three_way(
      payload->byteSpan().begin(), payload->byteSpan().end(),
      rhs.payload->byteSpan().begin(), rhs.payload->byteSpan().end());
}

HDRMetadata::HDRMetadata() = default;

HDRMetadata::HDRMetadata(const skhdr::Metadata& sk_hdr_metadata) {
  skhdr::ContentLightLevelInformation clli;
  if (sk_hdr_metadata.getContentLightLevelInformation(&clli)) {
    cta_861_3.emplace(clli.fMaxCLL, clli.fMaxFALL);
  }

  skhdr::MasteringDisplayColorVolume mdcv;
  if (sk_hdr_metadata.getMasteringDisplayColorVolume(&mdcv)) {
    smpte_st_2086.emplace(mdcv.fDisplayPrimaries,
                          mdcv.fMaximumDisplayMasteringLuminance,
                          mdcv.fMinimumDisplayMasteringLuminance);
  }
}

HDRMetadata::HDRMetadata(const HdrMetadataSmpteSt2086& smpte_st_2086,
                         const HdrMetadataCta861_3& cta_861_3)
    : smpte_st_2086(smpte_st_2086), cta_861_3(cta_861_3) {}
HDRMetadata::HDRMetadata(const HdrMetadataSmpteSt2086& smpte_st_2086)
    : smpte_st_2086(smpte_st_2086) {}
HDRMetadata::HDRMetadata(const HdrMetadataCta861_3& cta_861_3)
    : cta_861_3(cta_861_3) {}
HDRMetadata::HDRMetadata(const HDRMetadata& rhs) = default;
HDRMetadata& HDRMetadata::operator=(const HDRMetadata& rhs) = default;
HDRMetadata::~HDRMetadata() = default;

// static
float HDRMetadata::GetContentMaxLuminance(
    const std::optional<gfx::HDRMetadata>& metadata) {
  if (metadata.has_value()) {
    if (metadata->cta_861_3.has_value() &&
        metadata->cta_861_3->max_content_light_level > 0.f) {
      return metadata->cta_861_3->max_content_light_level;
    }
    if (metadata->smpte_st_2086.has_value() &&
        metadata->smpte_st_2086->luminance_max > 0.f) {
      return metadata->smpte_st_2086->luminance_max;
    }
  }
  return 1000.f;
}

// static
HDRMetadata HDRMetadata::PopulateUnspecifiedWithDefaults(
    const std::optional<gfx::HDRMetadata>& hdr_metadata) {
  constexpr HdrMetadataSmpteSt2086 kDefaults2086(SkNamedPrimaries::kRec2020,
                                                 1000.f, 0.f);

  if (!hdr_metadata) {
    return HDRMetadata(kDefaults2086);
  }

  HDRMetadata result = *hdr_metadata;
  if (!result.smpte_st_2086) {
    result.smpte_st_2086 = kDefaults2086;
    return result;
  }

  // If the gamut is unspecified, replace it with the default Rec2020.
  if (result.smpte_st_2086->primaries == SkNamedPrimariesExt::kInvalid) {
    result.smpte_st_2086->primaries = kDefaults2086.primaries;
  }

  // If the max luminance is unspecified, replace it with the default 1,000
  // nits.
  if (result.smpte_st_2086->luminance_max == 0.f) {
    result.smpte_st_2086->luminance_max = kDefaults2086.luminance_max;
  }

  return result;
}

std::string HDRMetadata::ToString() const {
  std::stringstream ss;
  ss << "{";
  if (smpte_st_2086) {
    ss << "smpte_st_2086:" << smpte_st_2086->ToString() << ", ";
  }
  if (cta_861_3) {
    ss << "cta_861_3:" << cta_861_3->ToString() << ", ";
  }
  if (ndwl) {
    ss << "ndwl:" << ndwl->ToString() << ", ";
  }
  if (extended_range) {
    ss << "extended_range:" << extended_range->ToString() << ", ";
  }
  if (agtm) {
    ss << "agtm:" << agtm->ToString() << ", ";
  }
  ss << "}";
  return ss.str();
}

}  // namespace gfx
