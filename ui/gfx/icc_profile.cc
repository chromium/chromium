// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/icc_profile.h"

#include <list>
#include <set>

#include "base/command_line.h"
#include "base/containers/mru_cache.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/synchronization/lock.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/third_party/skcms/skcms.h"
#include "ui/gfx/skia_color_space_util.h"

namespace gfx {

namespace {

static const size_t kMaxCachedICCProfiles = 16;

// An MRU cache mapping data to ICCProfile objects, to avoid re-parsing
// profiles every time they are read.
using DataToProfileCacheBase = base::MRUCache<std::vector<char>, ICCProfile>;
class DataToProfileCache : public DataToProfileCacheBase {
 public:
  DataToProfileCache() : DataToProfileCacheBase(kMaxCachedICCProfiles) {}
};
base::LazyInstance<DataToProfileCache>::Leaky g_data_to_profile_cache =
    LAZY_INSTANCE_INITIALIZER;

// Lock that must be held to access |g_data_to_profile_cache|.
base::LazyInstance<base::Lock>::Leaky g_icc_profile_lock =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

ICCProfile::Internals::AnalyzeResult ICCProfile::Internals::Initialize() {
  // Start out with no parametric data.
  if (data_.empty())
    return kICCNoProfile;

  // Parse the profile.
  skcms_ICCProfile profile;
  if (!skcms_Parse(data_.data(), data_.size(), &profile)) {
    DLOG(ERROR) << "Failed to parse ICC profile.";
    return kICCFailedToParse;
  }

  // We have seen many users with profiles that don't have a D50 white point.
  // Windows appears to detect these profiles, and not use them for OS drawing.
  // It still returns them when we query the system for the installed profile.
  // For consistency (and to match old behavior) we reject these profiles on
  // all platforms.
  // https://crbug.com/847024
  const skcms_Matrix3x3& m(profile.toXYZD50);
  float wX = m.vals[0][0] + m.vals[0][1] + m.vals[0][2];
  float wY = m.vals[1][0] + m.vals[1][1] + m.vals[1][2];
  float wZ = m.vals[2][0] + m.vals[2][1] + m.vals[2][2];
  static const float kD50_WhitePoint[3] = { 0.96420f, 1.00000f, 0.82491f };
  if (fabsf(wX - kD50_WhitePoint[0]) > 0.04f ||
      fabsf(wY - kD50_WhitePoint[1]) > 0.04f ||
      fabsf(wZ - kD50_WhitePoint[2]) > 0.04f) {
    return kICCFailedToParse;
  }

  // Extract the primary matrix, and assume that transfer function is sRGB until
  // we get something more precise.
  to_XYZD50_ = profile.toXYZD50;
  transfer_fn_ = SkNamedTransferFn::kSRGB;

  // Coerce it into a rasterization destination (if possible). If the profile
  // can't be approximated accurately, then use an sRGB transfer function and
  // return failure. We will continue to use the gamut from this profile.
  if (!skcms_MakeUsableAsDestinationWithSingleCurve(&profile)) {
    DLOG(ERROR) << "Parsed ICC profile but can't make usable as destination, "
                   "using sRGB gamma";
    return kICCFailedToMakeUsable;
  }

  // If SkColorSpace will treat the gamma as that of sRGB, then use the named
  // constants.
  sk_sp<SkColorSpace> sk_color_space = SkColorSpace::Make(profile);
  if (!sk_color_space) {
    DLOG(ERROR) << "Parsed ICC profile but cannot create SkColorSpace from it, "
                   "using sRGB gamma.";
    return kICCFailedToMakeUsable;
  }
  if (sk_color_space->gammaCloseToSRGB())
    return kICCExtractedMatrixAndTrFn;

  // We assume that if we accurately approximated the profile, then the
  // single-curve version (which may have higher error) is also okay. If we
  // want to maintain the distinction between accurate and inaccurate profiles,
  // we could check to see if the single-curve version is/ approximately equal
  // to the original (or to the multi-channel approximation).
  transfer_fn_ = profile.trc[0].parametric;
  return kICCExtractedMatrixAndTrFn;
}

ICCProfile::ICCProfile() = default;
ICCProfile::ICCProfile(ICCProfile&& other) = default;
ICCProfile::ICCProfile(const ICCProfile& other) = default;
ICCProfile& ICCProfile::operator=(ICCProfile&& other) = default;
ICCProfile& ICCProfile::operator=(const ICCProfile& other) = default;
ICCProfile::~ICCProfile() = default;

bool ICCProfile::operator==(const ICCProfile& other) const {
  if (!internals_ && !other.internals_)
    return true;
  if (internals_ && other.internals_) {
    return internals_->data_ == other.internals_->data_;
  }
  return false;
}

bool ICCProfile::operator!=(const ICCProfile& other) const {
  return !(*this == other);
}

bool ICCProfile::IsValid() const {
  return internals_ ? internals_->is_valid_ : false;
}

std::vector<char> ICCProfile::GetData() const {
  return internals_ ? internals_->data_ : std::vector<char>();
}

// static
ICCProfile ICCProfile::FromData(const void* data_as_void, size_t size) {
  const char* data_as_byte = reinterpret_cast<const char*>(data_as_void);
  std::vector<char> data(data_as_byte, data_as_byte + size);

  base::AutoLock lock(g_icc_profile_lock.Get());

  // See if there is already an entry with the same data. If so, return that
  // entry. If not, parse the data.
  ICCProfile icc_profile;
  auto found_by_data = g_data_to_profile_cache.Get().Get(data);
  if (found_by_data != g_data_to_profile_cache.Get().end()) {
    icc_profile = found_by_data->second;
  } else {
    icc_profile.internals_ = base::MakeRefCounted<Internals>(std::move(data));
  }

  // Insert the profile into all caches.
  g_data_to_profile_cache.Get().Put(icc_profile.internals_->data_, icc_profile);

  return icc_profile;
}

ColorSpace ICCProfile::GetColorSpace() const {
  if (!internals_)
    return ColorSpace();

  if (!internals_->is_valid_)
    return ColorSpace();

  return ColorSpace::CreateCustom(internals_->to_XYZD50_,
                                  internals_->transfer_fn_);
}

ColorSpace ICCProfile::GetPrimariesOnlyColorSpace() const {
  ColorSpace result = GetColorSpace();
  if (result.IsValid())
    result.transfer_ = ColorSpace::TransferID::IEC61966_2_1;
  return result;
}

bool ICCProfile::IsColorSpaceAccurate() const {
  if (!internals_)
    return false;

  if (!internals_->is_valid_)
    return false;

  return internals_->is_parametric_;
}

// static
ICCProfile ICCProfile::FromColorSpace(const ColorSpace& color_space) {
  if (!color_space.IsValid()) {
    return ICCProfile();
  }
  if (color_space.matrix_ != ColorSpace::MatrixID::RGB) {
    DLOG(ERROR) << "Not creating non-RGB ICCProfile";
    return ICCProfile();
  }
  if (color_space.range_ != ColorSpace::RangeID::FULL) {
    DLOG(ERROR) << "Not creating non-full-range ICCProfile";
    return ICCProfile();
  }
  skcms_Matrix3x3 to_XYZD50_matrix;
  color_space.GetPrimaryMatrix(&to_XYZD50_matrix);
  skcms_TransferFunction fn;
  if (!color_space.GetTransferFunction(&fn)) {
    DLOG(ERROR) << "Failed to get ColorSpace transfer function for ICCProfile.";
    return ICCProfile();
  }
  sk_sp<SkData> data = SkWriteICCProfile(fn, to_XYZD50_matrix);
  if (!data) {
    DLOG(ERROR) << "Failed to create SkICC.";
    return ICCProfile();
  }
  return FromData(data->data(), data->size());
}

ICCProfile::Internals::Internals(std::vector<char> data)
    : data_(std::move(data)) {
  // Early out for empty entries.
  if (data_.empty())
    return;

  // Parse the ICC profile
  analyze_result_ = Initialize();
  switch (analyze_result_) {
    case kICCExtractedMatrixAndTrFn:
      // Successfully and accurately extracted color space.
      is_valid_ = true;
      is_parametric_ = true;
      break;
    case kICCFailedToMakeUsable:
      // We have a usable gamut, but the transfer function may be messed up.
      is_valid_ = true;
      is_parametric_ = false;
      break;
    case kICCFailedToParse:
    case kICCNoProfile:
      // We can't use anything from this profile.
      is_valid_ = false;
      is_parametric_ = false;
      break;
  }
}

ICCProfile::Internals::~Internals() {}

void ICCProfile::HistogramDisplay(int64_t display_id) const {
  if (!internals_) {
    // If this is an uninitialized profile, histogram it using an empty profile,
    // so that we only histogram this display as empty once.
    FromData(nullptr, 0).HistogramDisplay(display_id);
  } else {
    internals_->HistogramDisplay(display_id);
  }
}

void ICCProfile::Internals::HistogramDisplay(int64_t display_id) {
  // Ensure that we histogram this profile only once per display id.
  if (histogrammed_display_ids_.count(display_id))
    return;
  histogrammed_display_ids_.insert(display_id);

  UMA_HISTOGRAM_ENUMERATION("Blink.ColorSpace.Destination.ICCResult",
                            analyze_result_);
}

}  // namespace gfx
