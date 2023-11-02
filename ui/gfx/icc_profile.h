// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_ICC_PROFILE_H_
#define UI_GFX_ICC_PROFILE_H_

#include <stdint.h>
#include <set>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "third_party/skia/modules/skcms/skcms.h"
#include "ui/gfx/color_space.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

namespace IPC {
template <class P>
struct ParamTraits;
}  // namespace IPC

namespace gfx {

// Used to represent a full ICC profile, usually retrieved from a monitor. It
// can be lossily compressed into a ColorSpace object. This structure should
// only be sent from higher-privilege processes to lower-privilege processes,
// as parsing this structure is not secure.
class COLOR_SPACE_EXPORT ICCProfile {
 public:
  ICCProfile();
  ICCProfile(ICCProfile&& other);
  ICCProfile(const ICCProfile& other);
  ICCProfile& operator=(ICCProfile&& other);
  ICCProfile& operator=(const ICCProfile& other);
  ~ICCProfile();
  bool operator==(const ICCProfile& other) const;
  bool operator!=(const ICCProfile& other) const;

  // Returns true if this profile was successfully parsed by SkICC and will
  // return a valid ColorSpace.
  bool IsValid() const;

  // Create directly from profile data. This function should be called only
  // in the browser process (and the results from there sent to other
  // processes).
  static ICCProfile FromData(const void* icc_profile, size_t size);

  // Create a profile for a color space. Returns an invalid profile if the
  // specified space is not expressable as an ICCProfile.
  static ICCProfile FromColorSpace(const gfx::ColorSpace& color_space);

  // Return a ColorSpace that best represents this ICCProfile.
  ColorSpace GetColorSpace() const;

  // Return a ColorSpace with the primaries from this ICCProfile and an
  // sRGB transfer function.
  ColorSpace GetPrimariesOnlyColorSpace() const;

  // Returns true if GetColorSpace returns an accurate representation of this
  // ICCProfile. This could be false if the result of GetColorSpace had to
  // approximate transfer functions.
  bool IsColorSpaceAccurate() const;

  // Return the data for the profile.
  std::vector<char> GetData() const;

 private:
  class Internals : public base::RefCountedThreadSafe<ICCProfile::Internals> {
   public:
    explicit Internals(std::vector<char>);

    const std::vector<char> data_;

    // True iff we can create a valid ColorSpace (and ColorTransform) from this
    // object. The transform may be LUT-based (using an SkColorSpaceXform to
    // compute the lut).
    bool is_valid_ = false;

    // True iff |to_XYZD50_| and |transfer_fn_| are accurate representations of
    // the data in this profile. In this case ColorTransforms created from this
    // profile will be analytic and not LUT-based.
    bool is_parametric_ = false;

    // The best-fit parametric primaries and transfer function.
    skcms_Matrix3x3 to_XYZD50_;
    skcms_TransferFunction transfer_fn_;

   protected:
    friend class base::RefCountedThreadSafe<ICCProfile::Internals>;
    void Initialize();
    virtual ~Internals();
  };
  scoped_refptr<Internals> internals_;

  FRIEND_TEST_ALL_PREFIXES(SimpleColorSpace, BT709toSRGBICC);
  FRIEND_TEST_ALL_PREFIXES(SimpleColorSpace, GetColorSpace);
  friend int ::LLVMFuzzerTestOneInput(const uint8_t*, size_t);
  friend class ColorSpace;
  friend class ColorTransformInternal;
  friend struct IPC::ParamTraits<gfx::ICCProfile>;
};

}  // namespace gfx

#endif  // UI_GFX_ICC_PROFILE_H_
