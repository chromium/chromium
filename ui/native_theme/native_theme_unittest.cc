// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/native_theme.h"

#include <ostream>
#include <tuple>

#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/native_theme/native_theme_color_id.h"

#if defined(OS_MAC)
#include "ui/color/mac/system_color_utils.h"
#endif

namespace ui {
namespace {

constexpr const char* kColorIdStringName[] = {
#define OP(enum_name) #enum_name
    NATIVE_THEME_COLOR_IDS
#undef OP
};

struct PrintableSkColor {
  bool operator==(const PrintableSkColor& other) const {
    return color == other.color;
  }

  bool operator!=(const PrintableSkColor& other) const {
    return !operator==(other);
  }

  const SkColor color;
};

std::ostream& operator<<(std::ostream& os, PrintableSkColor printable_color) {
  SkColor color = printable_color.color;
  return os << base::StringPrintf("SkColorARGB(0x%02x, 0x%02x, 0x%02x, 0x%02x)",
                                  SkColorGetA(color), SkColorGetR(color),
                                  SkColorGetG(color), SkColorGetB(color));
}

class NativeThemeRedirectedEquivalenceTest
    : public testing::TestWithParam<
          std::tuple<NativeTheme::ColorScheme, NativeTheme::ColorId>> {
 public:
  NativeThemeRedirectedEquivalenceTest() = default;

  static std::string ParamInfoToString(
      ::testing::TestParamInfo<std::tuple<NativeTheme::ColorScheme,
                                          NativeTheme::ColorId>> param_info) {
    auto param_tuple = param_info.param;
    return ColorSchemeToString(std::get<0>(param_tuple)) + "_With_" +
           ColorIdToString(std::get<1>(param_tuple));
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

  static std::string ColorIdToString(NativeTheme::ColorId id) {
    if (id >= NativeTheme::ColorId::kColorId_NumColors) {
      NOTREACHED() << "Invalid color value " << id;
      return "InvalidColorId";
    }
    return kColorIdStringName[id];
  }
};

std::pair<PrintableSkColor, PrintableSkColor> GetOriginalAndRedirected(
    NativeTheme::ColorId color_id,
    NativeTheme::ColorScheme color_scheme) {
  NativeTheme* native_theme = NativeTheme::GetInstanceForNativeUi();
  PrintableSkColor original{
      native_theme->GetSystemColor(color_id, color_scheme)};

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kColorProviderRedirection);
  PrintableSkColor redirected{
      native_theme->GetSystemColor(color_id, color_scheme)};

  return std::make_pair(original, redirected);
}

}  // namespace

TEST_P(NativeThemeRedirectedEquivalenceTest, NativeUiGetSystemColor) {
  auto param_tuple = GetParam();
  auto color_scheme = std::get<NativeTheme::ColorScheme>(param_tuple);
  auto color_id = std::get<NativeTheme::ColorId>(param_tuple);

  // Verifies that colors with and without the Color Provider are the same.
  auto pair = GetOriginalAndRedirected(color_id, color_scheme);
  auto original = pair.first;
  auto redirected = pair.second;
  EXPECT_EQ(original, redirected);
}

#if defined(OS_MAC)
TEST_P(NativeThemeRedirectedEquivalenceTest, NativeUiGetSystemColorWithTint) {
  auto param_tuple = GetParam();
  auto color_scheme = std::get<NativeTheme::ColorScheme>(param_tuple);
  auto color_id = std::get<NativeTheme::ColorId>(param_tuple);

  ScopedEnableGraphiteTint enable_graphite_tint;
  // Verifies that colors with and without the Color Provider are the same.
  auto pair = GetOriginalAndRedirected(color_id, color_scheme);
  auto original = pair.first;
  auto redirected = pair.second;
  EXPECT_EQ(original, redirected);
}
#endif

#define OP(enum_name) NativeTheme::ColorId::enum_name
INSTANTIATE_TEST_SUITE_P(
    ,
    NativeThemeRedirectedEquivalenceTest,
    ::testing::Combine(
        ::testing::Values(NativeTheme::ColorScheme::kLight,
                          NativeTheme::ColorScheme::kDark,
                          NativeTheme::ColorScheme::kPlatformHighContrast),
        ::testing::Values(NATIVE_THEME_COLOR_IDS)),
    NativeThemeRedirectedEquivalenceTest::ParamInfoToString);
#undef OP

}  // namespace ui
