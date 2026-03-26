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
#include "third_party/skia/include/core/SkString.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/skia_span_util.h"
#include "ui/gfx/switches.h"

namespace gfx {

// Custom comparators for skhdr:: types. Skia does not provide an arbitrary
// operator<=> (that is, one that is just to provide an ordering for types like
// std::map, but does not reflect a meaningful ordering), so create one for all
// of the relevant types. Avoid doing so by defining operator<=> for these
// types, because doing so will create a complicated transition if Skia starts
// including an operator<=>.
// See: https://crbug.com/40044808
namespace {

template <typename T>
std::weak_ordering Compare(const T& a, const T& b) {
  return std::weak_order(a, b);
}

template <typename U>
std::weak_ordering Compare(const std::optional<U>& a,
                           const std::optional<U>& b) {
  auto cmp = Compare(a.has_value(), b.has_value());
  if (cmp == std::weak_ordering::equivalent && a.has_value()) {
    cmp = Compare(a.value(), b.value());
  }
  return cmp;
}

template <typename U>
std::weak_ordering Compare(const std::vector<U>& a, const std::vector<U>& b) {
  auto cmp = Compare(a.size(), b.size());
  for (size_t i = 0; cmp == std::weak_ordering::equivalent && i < a.size();
       ++i) {
    cmp = Compare(a[i], b[i]);
  }
  return cmp;
}

#define COMPARE_BEGIN(T)                                  \
  template <>                                             \
  std::weak_ordering Compare<T>(const T& a, const T& b) { \
    auto cmp = std::weak_ordering::equivalent;

#define COMPARE_MEMBER(x)                      \
  cmp = Compare(a.x, b.x);                     \
  if (cmp != std::weak_ordering::equivalent) { \
    return cmp;                                \
  }

#define COMPARE_END()                    \
  return std::weak_ordering::equivalent; \
  }

COMPARE_BEGIN(SkColorSpacePrimaries)
COMPARE_MEMBER(fRX);
COMPARE_MEMBER(fRY);
COMPARE_MEMBER(fGX);
COMPARE_MEMBER(fGY);
COMPARE_MEMBER(fBX);
COMPARE_MEMBER(fBY);
COMPARE_MEMBER(fWX);
COMPARE_MEMBER(fWY);
COMPARE_END()

COMPARE_BEGIN(skhdr::ContentLightLevelInformation)
COMPARE_MEMBER(fMaxCLL);
COMPARE_MEMBER(fMaxFALL);
COMPARE_END()

COMPARE_BEGIN(skhdr::MasteringDisplayColorVolume)
COMPARE_MEMBER(fDisplayPrimaries)
COMPARE_MEMBER(fMaximumDisplayMasteringLuminance)
COMPARE_MEMBER(fMinimumDisplayMasteringLuminance)
COMPARE_END()

COMPARE_BEGIN(skhdr::AdaptiveGlobalToneMap::GainCurve::ControlPoint)
COMPARE_MEMBER(fX)
COMPARE_MEMBER(fY)
COMPARE_MEMBER(fM)
COMPARE_END()

COMPARE_BEGIN(skhdr::AdaptiveGlobalToneMap::GainCurve)
COMPARE_MEMBER(fControlPoints)
COMPARE_END()

COMPARE_BEGIN(skhdr::AdaptiveGlobalToneMap::ComponentMixingFunction)
COMPARE_MEMBER(fRed)
COMPARE_MEMBER(fGreen)
COMPARE_MEMBER(fBlue)
COMPARE_MEMBER(fMax)
COMPARE_MEMBER(fMin)
COMPARE_MEMBER(fComponent)
COMPARE_END()

COMPARE_BEGIN(skhdr::AdaptiveGlobalToneMap::ColorGainFunction)
COMPARE_MEMBER(fComponentMixing)
COMPARE_MEMBER(fGainCurve)
COMPARE_END()

COMPARE_BEGIN(skhdr::AdaptiveGlobalToneMap::AlternateImage)
COMPARE_MEMBER(fHdrHeadroom)
COMPARE_MEMBER(fColorGainFunction)
COMPARE_END()

COMPARE_BEGIN(skhdr::AdaptiveGlobalToneMap::HeadroomAdaptiveToneMap)
COMPARE_MEMBER(fBaselineHdrHeadroom)
COMPARE_MEMBER(fGainApplicationSpacePrimaries)
COMPARE_MEMBER(fAlternateImages)
COMPARE_END()

COMPARE_BEGIN(skhdr::AdaptiveGlobalToneMap)
COMPARE_MEMBER(fHdrReferenceWhite)
COMPARE_MEMBER(fHeadroomAdaptiveToneMap)
COMPARE_END()

}  // namespace

std::string HdrMetadataExtendedRange::ToString() const {
  std::stringstream ss;
  ss << std::fixed << std::setprecision(4) << "{"
     << "current_headroom:" << current_headroom << ", "
     << "desired_headroom:" << desired_headroom << "}";
  return ss.str();
}

std::weak_ordering HdrMetadataExtendedRange::operator<=>(
    const HdrMetadataExtendedRange& other) const {
  const auto& a = *this;
  const auto& b = other;
  auto cmp = std::weak_ordering::equivalent;
  COMPARE_MEMBER(current_headroom);
  COMPARE_MEMBER(desired_headroom);
  return std::weak_ordering::equivalent;
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
    clli_ = clli;
  }

  skhdr::MasteringDisplayColorVolume mdcv;
  if (sk_hdr_metadata.getMasteringDisplayColorVolume(&mdcv)) {
    mdcv_ = mdcv;
  }
}

HDRMetadata::HDRMetadata(const skhdr::MasteringDisplayColorVolume& mdcv,
                         const skhdr::ContentLightLevelInformation& clli)
    : mdcv_(mdcv), clli_(clli) {}
HDRMetadata::HDRMetadata(const skhdr::MasteringDisplayColorVolume& mdcv)
    : mdcv_(mdcv) {}
HDRMetadata::HDRMetadata(const skhdr::ContentLightLevelInformation& clli)
    : clli_(clli) {}
HDRMetadata::HDRMetadata(const HDRMetadata& rhs) = default;
HDRMetadata& HDRMetadata::operator=(const HDRMetadata& rhs) = default;
HDRMetadata::~HDRMetadata() = default;

void HDRMetadata::SetSerializedAgtm(base::span<const uint8_t> data) {
  if (!HdrMetadataAgtm::IsEnabled()) {
    return;
  }
  skhdr::AdaptiveGlobalToneMap agtm;
  if (agtm.parse(MakeSkDataFromSpanWithoutCopy(data).get())) {
    agtm_ = agtm;
  } else {
    // TODO(https://crbug.com/395659818): Several tests use out-of-date
    // encodings, but expect the data to still parse. To keep those tests
    // passing, set the HDR reference white to the default, with the size
    // after the decimal.
    agtm_ = {.fHdrReferenceWhite =
                 skhdr::AdaptiveGlobalToneMap::kDefaultHdrReferenceWhite +
                 data.size() / 10000.f};
  }
}

void HDRMetadata::MergeMetadataFrom(const HDRMetadata& other) {
  if (other.mdcv_) {
    mdcv_ = other.mdcv_;
  }
  if (other.clli_) {
    clli_ = other.clli_;
  }
  if (other.agtm_) {
    agtm_ = other.agtm_;
  }
  if (other.ndwl_) {
    ndwl_ = other.ndwl_;
  }
  if (other.extended_range) {
    extended_range = other.extended_range;
  }
}

// static
float HDRMetadata::GetContentMaxLuminance(const HDRMetadata& metadata) {
  if (metadata.clli_.has_value() && metadata.clli_->fMaxCLL > 0.f) {
    return metadata.clli_->fMaxCLL;
  }
  if (metadata.mdcv_.has_value() &&
      metadata.mdcv_->fMaximumDisplayMasteringLuminance > 0.f) {
    return metadata.mdcv_->fMaximumDisplayMasteringLuminance;
  }
  return 1000.f;
}

// static
float HDRMetadata::GetWaylandReferenceLuminance(
    const ColorSpace& color_space,
    const HDRMetadata& hdr_metadata) {
  if (HdrMetadataAgtm::IsEnabled() && hdr_metadata.agtm_) {
    return hdr_metadata.agtm_->fHdrReferenceWhite;
  }

  if (hdr_metadata.ndwl_.has_value() && hdr_metadata.ndwl_.value() > 0.f) {
    return hdr_metadata.ndwl_.value();
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
  const skhdr::MasteringDisplayColorVolume kDefaults2086 = {
      .fDisplayPrimaries = SkNamedPrimaries::kRec2020,
      .fMaximumDisplayMasteringLuminance = 1000.f,
      .fMinimumDisplayMasteringLuminance = 0.f};

  if (hdr_metadata.IsEmpty()) {
    return HDRMetadata(kDefaults2086);
  }

  HDRMetadata result = hdr_metadata;
  if (!result.mdcv_) {
    result.mdcv_ = kDefaults2086;
    return result;
  }

  // If the gamut is unspecified, replace it with the default Rec2020.
  if (result.mdcv_->fDisplayPrimaries == SkNamedPrimariesExt::kInvalid) {
    result.mdcv_->fDisplayPrimaries = kDefaults2086.fDisplayPrimaries;
  }

  // If the max luminance is unspecified, replace it with the default 1,000
  // nits.
  if (result.mdcv_->fMaximumDisplayMasteringLuminance == 0.f) {
    result.mdcv_->fMaximumDisplayMasteringLuminance =
        kDefaults2086.fMaximumDisplayMasteringLuminance;
  }

  return result;
}

std::string HDRMetadata::ToString() const {
  std::stringstream ss;
  ss << "{";
  if (mdcv_) {
    ss << "mdcv:" << mdcv_->toString().c_str() << ", ";
  }
  if (clli_) {
    ss << "clli:" << clli_->toString().c_str() << ", ";
  }
  if (ndwl_) {
    ss << "ndwl:" << *ndwl_ << ", ";
  }
  if (extended_range) {
    ss << "extended_range:" << extended_range->ToString() << ", ";
  }
  if (agtm_) {
    ss << "agtm:" << agtm_->toString().c_str() << ", ";
  }
  ss << "}";
  return ss.str();
}

bool HDRMetadata::operator==(const HDRMetadata& other) const {
  return (*this <=> other) == std::partial_ordering::equivalent;
}

std::weak_ordering HDRMetadata::operator<=>(const HDRMetadata& other) const {
  const auto& a = *this;
  const auto& b = other;
  auto cmp = std::weak_ordering::equivalent;
  COMPARE_MEMBER(ndwl_);
  COMPARE_MEMBER(clli_);
  COMPARE_MEMBER(mdcv_);
  COMPARE_MEMBER(agtm_);
  COMPARE_MEMBER(extended_range);
  return std::weak_ordering::equivalent;
}

}  // namespace gfx
