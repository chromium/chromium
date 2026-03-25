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
    skhdr::AdaptiveGlobalToneMap agtm;
    if (agtm.parse(hdr_metadata.agtm_.get())) {
      return agtm.fHdrReferenceWhite;
    }
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
    ss << "agtm:present, ";
  }
  ss << "}";
  return ss.str();
}

bool HDRMetadata::operator==(const HDRMetadata& other) const {
  return (*this <=> other) == std::partial_ordering::equivalent;
}

std::partial_ordering HDRMetadata::operator<=>(const HDRMetadata& other) const {
  auto cmp = std::partial_ordering::equivalent;
  {
    const auto& a = *this;
    const auto& b = other;
    cmp = std::tie(a.ndwl_, a.extended_range) <=>
          std::tie(b.ndwl_, b.extended_range);
    if (cmp != std::partial_ordering::equivalent) {
      return cmp;
    }
  }

  cmp = SkDataCompare(agtm_, other.agtm_);
  if (cmp != std::partial_ordering::equivalent) {
    return cmp;
  }

  // Skia does not provide an arbitrary operator<=> (that is, one that is just
  // to provide an ordering for types like std::map, but does not reflect a
  // meaningful ordering), so create one here.
  cmp = mdcv_.has_value() <=> other.mdcv_.has_value();
  if (cmp != std::partial_ordering::equivalent) {
    return cmp;
  }
  if (mdcv_.has_value()) {
    const auto& a = *mdcv_;
    const auto& b = *other.mdcv_;
    cmp = std::tie(a.fDisplayPrimaries.fRX, a.fDisplayPrimaries.fRY,
                   a.fDisplayPrimaries.fGX, a.fDisplayPrimaries.fGY,
                   a.fDisplayPrimaries.fBX, a.fDisplayPrimaries.fBY,
                   a.fDisplayPrimaries.fWX, a.fDisplayPrimaries.fWY,
                   a.fMaximumDisplayMasteringLuminance,
                   a.fMinimumDisplayMasteringLuminance) <=>
          std::tie(b.fDisplayPrimaries.fRX, b.fDisplayPrimaries.fRY,
                   b.fDisplayPrimaries.fGX, b.fDisplayPrimaries.fGY,
                   b.fDisplayPrimaries.fBX, b.fDisplayPrimaries.fBY,
                   b.fDisplayPrimaries.fWX, b.fDisplayPrimaries.fWY,
                   b.fMaximumDisplayMasteringLuminance,
                   b.fMinimumDisplayMasteringLuminance);
    if (cmp != std::partial_ordering::equivalent) {
      return cmp;
    }
  }
  cmp = clli_.has_value() <=> other.clli_.has_value();
  if (cmp != std::partial_ordering::equivalent) {
    return cmp;
  }
  if (clli_.has_value()) {
    const auto& a = *clli_;
    const auto& b = *other.clli_;
    cmp = std::tie(a.fMaxCLL, a.fMaxFALL) <=> std::tie(b.fMaxCLL, b.fMaxFALL);
    if (cmp != std::partial_ordering::equivalent) {
      return cmp;
    }
  }
  return cmp;
}

}  // namespace gfx
