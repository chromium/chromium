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
#include "ui/gfx/color_space.h"
#include "ui/gfx/skia_span_util.h"
#include "ui/gfx/switches.h"

namespace gfx {

namespace {

std::weak_ordering SkDataCompare(const sk_sp<const SkData>& a,
                                 const sk_sp<const SkData>& b) {
  if (!a && !b) {
    return std::weak_ordering::equivalent;
  }
  if (!a) {
    return std::weak_ordering::less;
  }
  if (!b) {
    return std::weak_ordering::greater;
  }
  if (auto size_cmp = a->size() <=> b->size();
      size_cmp != std::weak_ordering::equivalent) {
    return size_cmp;
  }
  return SkDataToSpan(a) <=> SkDataToSpan(b);
}

}  // namespace

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

// static
bool HdrMetadataAgtm::IsEnabled() {
  static bool result = base::FeatureList::IsEnabled(features::kHdrAgtm);
  return result;
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
float HDRMetadata::GetContentMaxLuminance(const HDRMetadata& metadata) {
  if (metadata.cta_861_3.has_value() &&
      metadata.cta_861_3->max_content_light_level > 0.f) {
    return metadata.cta_861_3->max_content_light_level;
  }
  if (metadata.smpte_st_2086.has_value() &&
      metadata.smpte_st_2086->luminance_max > 0.f) {
    return metadata.smpte_st_2086->luminance_max;
  }
  return 1000.f;
}

// static
float HDRMetadata::GetWaylandReferenceLuminance(
    const ColorSpace& color_space,
    const HDRMetadata& hdr_metadata) {
  if (HdrMetadataAgtm::IsEnabled() && hdr_metadata.agtm_) {
    skhdr::AdaptiveGlobalToneMap agtm;
    if (agtm.parse(hdr_metadata.agtm_.get())) {
      return agtm.fHdrReferenceWhite;
    }
  }

  if (hdr_metadata.ndwl.has_value() && hdr_metadata.ndwl->nits > 0.f) {
    return hdr_metadata.ndwl->nits;
  }

  if (color_space.GetTransferID() == ColorSpace::TransferID::PQ ||
      color_space.GetTransferID() == ColorSpace::TransferID::HLG) {
    auto sk_color_space = color_space.ToSkColorSpace();
    skcms_TransferFunction transfer_fn;
    sk_color_space->transferFn(&transfer_fn);
    return transfer_fn.a;
  }

  return ColorSpace::kDefaultSDRWhiteLevel;
}

// static
HDRMetadata HDRMetadata::PopulateUnspecifiedWithDefaults(
    const HDRMetadata& hdr_metadata) {
  constexpr HdrMetadataSmpteSt2086 kDefaults2086(SkNamedPrimaries::kRec2020,
                                                 1000.f, 0.f);

  if (hdr_metadata.IsEmpty()) {
    return HDRMetadata(kDefaults2086);
  }

  HDRMetadata result = hdr_metadata;
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
  if (agtm_) {
    ss << "agtm:present, ";
  }
  ss << "}";
  return ss.str();
}

bool HDRMetadata::operator==(const HDRMetadata& other) const {
  if (std::tie(smpte_st_2086, cta_861_3, ndwl, extended_range) !=
      std::tie(other.smpte_st_2086, other.cta_861_3, other.ndwl,
               other.extended_range)) {
    return false;
  }
  return SkData::Equals(agtm_.get(), other.agtm_.get());
}

std::partial_ordering HDRMetadata::operator<=>(const HDRMetadata& other) const {
  auto cmp = std::tie(smpte_st_2086, cta_861_3, ndwl, extended_range) <=>
             std::tie(other.smpte_st_2086, other.cta_861_3, other.ndwl,
                      other.extended_range);
  if (cmp != std::partial_ordering::equivalent) {
    return cmp;
  }
  return SkDataCompare(agtm_, other.agtm_);
}

}  // namespace gfx
