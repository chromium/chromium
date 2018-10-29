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
#include "third_party/skia/third_party/skcms/skcms.h"
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

// An MRU cache mapping IDs to ICCProfile objects. This is necessary for
// constructing LUT-based color transforms. In particular, it is used to look
// up the SkColorSpace for ColorSpace objects that are not parametric, so that
// that SkColorSpace may be used to construct the LUT.
using IdToProfileCacheBase = base::MRUCache<uint64_t, ICCProfile>;
class IdToProfileCache : public IdToProfileCacheBase {
 public:
  IdToProfileCache() : IdToProfileCacheBase(kMaxCachedICCProfiles) {}
};
base::LazyInstance<IdToProfileCache>::Leaky g_id_to_profile_cache =
    LAZY_INSTANCE_INITIALIZER;

// The next id to assign to a color profile.
uint64_t g_next_unused_id = 1;

// Lock that must be held to access |g_next_unused_id|.
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

  // Coerce it into a rasterization destination (if possible). If the profile
  // can't be approximated accurately, skcms will not allow transforming to it,
  // and this will fail.
  if (!skcms_MakeUsableAsDestinationWithSingleCurve(&profile)) {
    DLOG(ERROR) << "Parsed ICC profile but can't make usable as destination.";
    return kICCFailedToMakeUsable;
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

  // Create an SkColorSpace from the profile. This should always succeed after
  // calling MakeUsableAsDestinationWithSingleCurve.
  sk_color_space_ = SkColorSpace::Make(profile);
  DCHECK(sk_color_space_);

  // Extract the primary matrix and transfer function
  to_XYZD50_.set3x3RowMajorf(&profile.toXYZD50.vals[0][0]);
  memcpy(&transfer_fn_, &profile.trc[0].parametric, sizeof(transfer_fn_));

  // We assume that if we accurately approximated the profile, then the
  // single-curve version (which may have higher error) is also okay. If we
  // want to maintain the distinction between accurate and inaccurate profiles,
  // we could check to see if the single-curve version is/ approximately equal
  // to the original (or to the multi-channel approximation).
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
    return internals_->data_ == other.internals_->data_ &&
           internals_->id_ == other.internals_->id_;
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
ICCProfile ICCProfile::FromData(const void* data, size_t size) {
  return FromDataWithId(data, size, 0);
}

// static
ICCProfile ICCProfile::FromDataWithId(const void* data_as_void,
                                      size_t size,
                                      uint64_t new_profile_id) {
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
    icc_profile.internals_ =
        base::MakeRefCounted<Internals>(std::move(data), new_profile_id);
  }

  // Insert the profile into all caches.
  if (icc_profile.internals_->id_)
    g_id_to_profile_cache.Get().Put(icc_profile.internals_->id_, icc_profile);
  g_data_to_profile_cache.Get().Put(icc_profile.internals_->data_, icc_profile);

  return icc_profile;
}

ColorSpace ICCProfile::GetColorSpace() const {
  if (!internals_)
    return ColorSpace();

  if (!internals_->is_valid_)
    return ColorSpace();

  // TODO(ccameron): Compute a reasonable approximation instead of always
  // falling back to sRGB.
  ColorSpace color_space =
      internals_->sk_color_space_->isSRGB()
          ? ColorSpace::CreateSRGB()
          : ColorSpace::CreateCustom(internals_->to_XYZD50_,
                                     internals_->transfer_fn_);
  color_space.icc_profile_id_ = internals_->id_;
  return color_space;
}

// static
ICCProfile ICCProfile::FromParametricColorSpace(const ColorSpace& color_space) {
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
  if (color_space.icc_profile_id_) {
    DLOG(ERROR) << "Not creating non-parametric ICCProfile";
    return ICCProfile();
  }

  SkMatrix44 to_XYZD50_matrix;
  color_space.GetPrimaryMatrix(&to_XYZD50_matrix);
  SkColorSpaceTransferFn fn;
  if (!color_space.GetTransferFunction(&fn)) {
    DLOG(ERROR) << "Failed to get ColorSpace transfer function for ICCProfile.";
    return ICCProfile();
  }
  sk_sp<SkData> data = SkICC::WriteToICC(fn, to_XYZD50_matrix);
  if (!data) {
    DLOG(ERROR) << "Failed to create SkICC.";
    return ICCProfile();
  }
  return FromDataWithId(data->data(), data->size(), 0);
}

// static
sk_sp<SkColorSpace> ICCProfile::GetSkColorSpaceFromId(uint64_t id) {
  base::AutoLock lock(g_icc_profile_lock.Get());
  auto found = g_id_to_profile_cache.Get().Get(id);
  if (found == g_id_to_profile_cache.Get().end()) {
    DLOG(ERROR) << "Failed to find ICC profile with SkColorSpace from id.";
    return nullptr;
  }
  return found->second.internals_->sk_color_space_;
}

ICCProfile::Internals::Internals(std::vector<char> data, uint64_t id)
    : data_(std::move(data)), id_(id) {
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
    case kICCFailedToParse:
    case kICCNoProfile:
    case kICCFailedToMakeUsable:
      // Can't even use this color space as a LUT.
      is_valid_ = false;
      is_parametric_ = false;
      break;
  }

  if (id_) {
    // If |id_| has been set here, then it was specified via sending an
    // ICCProfile over IPC. Ensure that the computation of |is_valid_| and
    // |is_parametric_| match the analysis done in the sending process.
    DCHECK(is_valid_ && !is_parametric_);
  } else {
    // If this profile is not parametric, assign it an id so that we can look it
    // up from a ColorSpace. This path should only be hit in the browser
    // process.
    if (is_valid_ && !is_parametric_) {
      id_ = g_next_unused_id++;
    }
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
