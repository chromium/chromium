// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme.h"

#include <ostream>
#include <tuple>

#include "base/notreached.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/native_theme/native_theme_color_id.h"
#include "ui/native_theme/test/color_utils.h"

#if defined(OS_MAC)
#include "ui/color/mac/system_color_utils.h"
#endif

namespace {

enum class ContrastMode { kNonHighContrast, kHighContrast };

}  // namespace

namespace ui {
namespace {

class NativeThemeRedirectedEquivalenceTest
    : public testing::TestWithParam<std::tuple<NativeTheme::ColorScheme,
                                               ContrastMode,
                                               NativeTheme::ColorId>> {
 public:
  NativeThemeRedirectedEquivalenceTest() = default;

  static std::string ParamInfoToString(
      ::testing::TestParamInfo<std::tuple<NativeTheme::ColorScheme,
                                          ContrastMode,
                                          NativeTheme::ColorId>> param_info) {
    auto param_tuple = param_info.param;
    return ColorSchemeToString(
               std::get<NativeTheme::ColorScheme>(param_tuple)) +
           ContrastModeToString(std::get<ContrastMode>(param_tuple)) +
           "_With_" +
           test::ColorIdToString(std::get<NativeTheme::ColorId>(param_tuple));
  }

 private:
  static std::string ColorSchemeToString(NativeTheme::ColorScheme scheme) {
    switch (scheme) {
      case NativeTheme::ColorScheme::kDefault:
        NOTREACHED()
            << "Cannot unit test kDefault as it depends on machine state.";
        return "InvalidColorScheme";
      case NativeTheme::ColorScheme::kLight:
        return "kLight";
      case NativeTheme::ColorScheme::kDark:
        return "kDark";
      case NativeTheme::ColorScheme::kPlatformHighContrast:
        return "kPlatformHighContrast";
    }
  }

  static std::string ContrastModeToString(ContrastMode contrast_mode) {
    switch (contrast_mode) {
      case ContrastMode::kNonHighContrast:
        return "";
      case ContrastMode::kHighContrast:
        return "HighContrast";
      default:
        NOTREACHED();
        return "InvalidContrastMode";
    }
  }
};

std::pair<test::PrintableSkColor, test::PrintableSkColor>
GetOriginalAndRedirected(NativeTheme::ColorId color_id,
                         NativeTheme::ColorScheme color_scheme,
                         ContrastMode contrast_mode) {
  NativeTheme* native_theme = NativeTheme::GetInstanceForNativeUi();

  if (contrast_mode == ContrastMode::kHighContrast) {
#if defined(OS_WIN)
    color_scheme = NativeTheme::ColorScheme::kPlatformHighContrast;
#endif
    native_theme->set_preferred_contrast(NativeTheme::PreferredContrast::kMore);
  }

  test::PrintableSkColor original{
      native_theme->GetSystemColor(color_id, color_scheme)};

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kColorProviderRedirection);
  test::PrintableSkColor redirected{
      native_theme->GetSystemColor(color_id, color_scheme)};
  native_theme->set_preferred_contrast(
      NativeTheme::PreferredContrast::kNoPreference);

  return std::make_pair(original, redirected);
}

}  // namespace

TEST_P(NativeThemeRedirectedEquivalenceTest, NativeUiGetSystemColor) {
  auto param_tuple = GetParam();
  auto color_scheme = std::get<NativeTheme::ColorScheme>(param_tuple);
  auto contrast_mode = std::get<ContrastMode>(param_tuple);
  auto color_id = std::get<NativeTheme::ColorId>(param_tuple);

  // Verifies that colors with and without the Color Provider are the same.
  auto pair = GetOriginalAndRedirected(color_id, color_scheme, contrast_mode);
  auto original = pair.first;
  auto redirected = pair.second;
  EXPECT_EQ(original, redirected);
}

#if defined(OS_MAC)
TEST_P(NativeThemeRedirectedEquivalenceTest, NativeUiGetSystemColorWithTint) {
  auto param_tuple = GetParam();
  auto color_scheme = std::get<NativeTheme::ColorScheme>(param_tuple);
  auto contrast_mode = std::get<ContrastMode>(param_tuple);
  auto color_id = std::get<NativeTheme::ColorId>(param_tuple);

  ScopedEnableGraphiteTint enable_graphite_tint;
  // Verifies that colors with and without the Color Provider are the same.
  auto pair = GetOriginalAndRedirected(color_id, color_scheme, contrast_mode);
  auto original = pair.first;
  auto redirected = pair.second;
  EXPECT_EQ(original, redirected);
}
#endif

#define OP(enum_name) NativeTheme::ColorId::enum_name
INSTANTIATE_TEST_SUITE_P(
    ,
    NativeThemeRedirectedEquivalenceTest,
    ::testing::Combine(::testing::Values(NativeTheme::ColorScheme::kLight,
                                         NativeTheme::ColorScheme::kDark),
                       ::testing::Values(ContrastMode::kNonHighContrast,
                                         ContrastMode::kHighContrast),
                       ::testing::Values(NATIVE_THEME_COLOR_IDS)),
    NativeThemeRedirectedEquivalenceTest::ParamInfoToString);
#undef OP

}  // namespace ui
