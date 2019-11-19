// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/icc_profile.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/skia_color_space_util.h"
#include "ui/gfx/test/icc_profiles.h"

namespace gfx {

TEST(ICCProfile, Conversions) {
  ICCProfile icc_profile = ICCProfileForTestingColorSpin();
  ColorSpace color_space_from_icc_profile = icc_profile.GetColorSpace();

  ICCProfile icc_profile_from_color_space =
      ICCProfile::FromColorSpace(color_space_from_icc_profile);
  EXPECT_TRUE(icc_profile_from_color_space.IsValid());
  EXPECT_NE(icc_profile, icc_profile_from_color_space);
}

TEST(ICCProfile, SRGB) {
  ICCProfile icc_profile = ICCProfileForTestingSRGB();
  ColorSpace color_space = ColorSpace::CreateSRGB();
  sk_sp<SkColorSpace> sk_color_space = SkColorSpace::MakeSRGB();

  // The ICC profile parser should note that this is SRGB.
  EXPECT_EQ(icc_profile.GetColorSpace(), ColorSpace::CreateSRGB());
  EXPECT_EQ(icc_profile.GetColorSpace().ToSkColorSpace().get(),
            sk_color_space.get());
  // The generated color space should recognize that this is SRGB.
  EXPECT_EQ(color_space.ToSkColorSpace().get(), sk_color_space.get());
}

TEST(ICCProfile, Equality) {
  ICCProfile spin_profile = ICCProfileForTestingColorSpin();
  ICCProfile adobe_profile = ICCProfileForTestingAdobeRGB();
  EXPECT_TRUE(spin_profile == spin_profile);
  EXPECT_FALSE(spin_profile != spin_profile);
  EXPECT_FALSE(spin_profile == adobe_profile);
  EXPECT_TRUE(spin_profile != adobe_profile);

  gfx::ColorSpace spin_space = spin_profile.GetColorSpace();
  gfx::ColorSpace adobe_space = adobe_profile.GetColorSpace();
  EXPECT_TRUE(spin_space == spin_space);
  EXPECT_FALSE(spin_space != spin_space);
  EXPECT_FALSE(spin_space == adobe_space);
  EXPECT_TRUE(spin_space != adobe_space);

  EXPECT_TRUE(!!spin_space.ToSkColorSpace());
  EXPECT_TRUE(!!adobe_space.ToSkColorSpace());
  EXPECT_FALSE(SkColorSpace::Equals(
      spin_space.ToSkColorSpace().get(),
      adobe_space.ToSkColorSpace().get()));
}

TEST(ICCProfile, ParametricVersusExactInaccurate) {
  // This ICC profile has three transfer functions that differ significantly,
  // but ICCProfiles are always either invalid or considered accurate (and in
  // this case, each curve is approximated, so the profile is "accurate").
  // See comments in ICCProfile::Internals::Analyze.
  ICCProfile multi_tr_fn = ICCProfileForTestingNoAnalyticTrFn();
  EXPECT_TRUE(multi_tr_fn.IsColorSpaceAccurate());

  // We are capable of generating a parametric approximation.
  ICCProfile profile;
  profile = ICCProfile::FromColorSpace(multi_tr_fn.GetColorSpace());
  EXPECT_TRUE(profile.IsValid());
  EXPECT_NE(profile, multi_tr_fn);
}

TEST(ICCProfile, ParametricVersusExactOvershoot) {
  // This ICC profile has a transfer function with T(1) that is greater than 1
  // in the approximation, but is still close enough to be considered accurate.
  ICCProfile overshoot = ICCProfileForTestingOvershoot();
  EXPECT_TRUE(overshoot.IsColorSpaceAccurate());

  ICCProfile profile;
  profile = ICCProfile::FromColorSpace(overshoot.GetColorSpace());
  EXPECT_TRUE(profile.IsValid());
  EXPECT_NE(profile, overshoot);
}

TEST(ICCProfile, ParametricVersusExactAdobe) {
  // This ICC profile is precisely represented by the parametric color space.
  ICCProfile accurate = ICCProfileForTestingAdobeRGB();
  EXPECT_TRUE(accurate.IsColorSpaceAccurate());

  ICCProfile profile;
  profile = ICCProfile::FromColorSpace(accurate.GetColorSpace());
  EXPECT_TRUE(profile.IsValid());
  EXPECT_NE(profile, accurate);
}

TEST(ICCProfile, ParametricVersusExactA2B) {
  // This ICC profile has only an A2B representation. We cannot transform to
  // A2B only ICC profiles, so this should be marked as invalid.
  ICCProfile a2b = ICCProfileForTestingA2BOnly();
  EXPECT_FALSE(a2b.GetColorSpace().IsValid());

  // Even though it is invalid, it should not be equal to the empty constructor
  EXPECT_NE(a2b, gfx::ICCProfile());
}

TEST(ICCProfile, GarbageData) {
  std::vector<char> bad_data(10 * 1024);
  const char* bad_data_string = "deadbeef";
  for (size_t i = 0; i < bad_data.size(); ++i)
    bad_data[i] = bad_data_string[i % 8];
  ICCProfile garbage_profile =
      ICCProfile::FromData(bad_data.data(), bad_data.size());
  EXPECT_FALSE(garbage_profile.IsValid());
  EXPECT_FALSE(garbage_profile.GetColorSpace().IsValid());

  ICCProfile default_ctor_profile;
  EXPECT_FALSE(default_ctor_profile.IsValid());
  EXPECT_FALSE(default_ctor_profile.GetColorSpace().IsValid());
}

TEST(ICCProfile, GenericRGB) {
  ColorSpace icc_profile = ICCProfileForTestingGenericRGB().GetColorSpace();
  ColorSpace color_space(ColorSpace::PrimaryID::APPLE_GENERIC_RGB,
                         ColorSpace::TransferID::GAMMA18);

  SkMatrix44 icc_profile_matrix;
  SkMatrix44 color_space_matrix;

  icc_profile.GetPrimaryMatrix(&icc_profile_matrix);
  color_space.GetPrimaryMatrix(&color_space_matrix);

  SkMatrix44 eye;
  icc_profile_matrix.invert(&eye);
  eye.postConcat(color_space_matrix);
  EXPECT_TRUE(SkMatrixIsApproximatelyIdentity(eye));
}

}  // namespace gfx
