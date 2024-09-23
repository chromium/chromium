// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/color/sys_color_mixer.h"

#include <tuple>

#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_provider_key.h"
#include "ui/color/color_provider_utils.h"
#include "ui/color/color_recipe.h"
#include "ui/color/color_transform.h"
#include "ui/color/ref_color_mixer.h"
#include "ui/gfx/color_palette.h"

namespace ui {
namespace {

class SysColorMixerTest
    : public testing::Test,
      public testing::WithParamInterface<std::tuple<bool, SkColor>> {
 public:
  // testing::Test:
  void SetUp() override {
    Test::SetUp();
    ColorProviderKey key;
    key.color_mode = std::get<0>(GetParam())
                         ? ColorProviderKey::ColorMode::kDark
                         : ColorProviderKey::ColorMode::kLight;
    key.user_color = std::get<1>(GetParam());
    key.user_color_source = ColorProviderKey::UserColorSource::kAccent;

    AddRefColorMixer(&color_provider_, key);
    AddSysColorMixer(&color_provider_, key);
  }

 protected:
  ColorProvider color_provider_;
};

TEST_P(SysColorMixerTest, SysColorContrast) {
  constexpr ColorId minimum_visible_contrasting_ids[][2] = {
      {kColorSysStateTextHighlight, kColorSysBase},
      {kColorSysStateTextHighlight, kColorSysOmniboxContainer},
  };
  constexpr ColorId minimum_readable_contrasting_ids[][2] = {
      {kColorSysStateOnTextHighlight, kColorSysStateTextHighlight},
  };
  auto check_sufficient_contrast = [&](ColorId id1, ColorId id2,
                                       float expected_contrast_ratio) {
    const SkColor color1 = color_provider_.GetColor(id1);
    const SkColor color2 = color_provider_.GetColor(id2);
    const float contrast = color_utils::GetContrastRatio(color1, color2);
    EXPECT_GE(contrast, expected_contrast_ratio)
        << ColorIdName(id1) << " - " << SkColorName(color1) << "\n"
        << ColorIdName(id2) << " - " << SkColorName(color2);
  };

  for (const ColorId* ids : minimum_visible_contrasting_ids) {
    check_sufficient_contrast(ids[0], ids[1],
                              color_utils::kMinimumVisibleContrastRatio);
  }
  for (const ColorId* ids : minimum_readable_contrasting_ids) {
    check_sufficient_contrast(ids[0], ids[1],
                              color_utils::kMinimumReadableContrastRatio);
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SysColorMixerTest,
    testing::Combine(testing::Bool(),
                     testing::Values(gfx::kGoogleBlue500,
                                     gfx::kGoogleRed500,
                                     gfx::kGoogleGreen500,
                                     gfx::kGoogleYellow500,
                                     gfx::kGoogleGrey500,
                                     gfx::kGoogleOrange500,
                                     gfx::kGooglePink500,
                                     gfx::kGooglePurple500,
                                     gfx::kGoogleCyan500,
                                     SK_ColorBLACK,
                                     SK_ColorWHITE)),
    [](const testing::TestParamInfo<std::tuple<bool, SkColor>>& info) {
      return base::StringPrintf("%s_%s",
                                std::get<0>(info.param) ? "dark" : "light",
                                SkColorName(std::get<1>(info.param)).c_str());
    });

}  // namespace
}  // namespace ui
