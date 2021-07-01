// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>

#include "base/check.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/gtk/gtk_ui_factory.h"
#include "ui/native_theme/native_theme_color_id.h"
#include "ui/native_theme/test/color_utils.h"
#include "ui/views/linux_ui/linux_ui.h"

namespace gtk {

namespace {

class NativeThemeGtkRedirectedEquivalenceTest
    : public testing::TestWithParam<
          std::tuple<ui::NativeTheme::ColorScheme, ui::NativeTheme::ColorId>> {
 public:
  NativeThemeGtkRedirectedEquivalenceTest() { gtk_ui_ = BuildGtkUi(); }

  static std::string ParamInfoToString(
      ::testing::TestParamInfo<
          std::tuple<ui::NativeTheme::ColorScheme, ui::NativeTheme::ColorId>>
          param_info) {
    auto param_tuple = param_info.param;
    return ColorSchemeToString(
               std::get<ui::NativeTheme::ColorScheme>(param_tuple)) +
           "_With_" +
           ui::test::ColorIdToString(
               std::get<ui::NativeTheme::ColorId>(param_tuple));
  }

 protected:
  views::LinuxUI* gtk_ui() { return gtk_ui_.get(); }

 private:
  static std::string ColorSchemeToString(ui::NativeTheme::ColorScheme scheme) {
    switch (scheme) {
      case ui::NativeTheme::ColorScheme::kDefault:
        NOTREACHED()
            << "Cannot unit test kDefault as it depends on machine state.";
        return "InvalidColorScheme";
      case ui::NativeTheme::ColorScheme::kLight:
        return "kLight";
      case ui::NativeTheme::ColorScheme::kDark:
        return "kDark";
      case ui::NativeTheme::ColorScheme::kPlatformHighContrast:
        return "kPlatformHighContrast";
    }
  }

  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<views::LinuxUI> gtk_ui_;
};

}  // namespace

TEST_P(NativeThemeGtkRedirectedEquivalenceTest, GetSystemColor) {
  auto param_tuple = GetParam();
  auto color_scheme = std::get<ui::NativeTheme::ColorScheme>(param_tuple);
  auto color_id = std::get<ui::NativeTheme::ColorId>(param_tuple);

  // Verifies that colors with and without the Color Provider are the same.
  auto* native_theme_gtk = gtk_ui()->GetNativeTheme(nullptr);

  ui::test::PrintableSkColor original{
      native_theme_gtk->GetSystemColor(color_id, color_scheme)};

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kColorProviderRedirection);
  ui::test::PrintableSkColor redirected{
      native_theme_gtk->GetSystemColor(color_id, color_scheme)};

  EXPECT_EQ(original, redirected);
}

#define OP(enum_name) ui::NativeTheme::ColorId::enum_name
INSTANTIATE_TEST_SUITE_P(
    ,
    NativeThemeGtkRedirectedEquivalenceTest,
    ::testing::Combine(
        ::testing::Values(ui::NativeTheme::ColorScheme::kLight,
                          ui::NativeTheme::ColorScheme::kDark,
                          ui::NativeTheme::ColorScheme::kPlatformHighContrast),
        ::testing::Values(NATIVE_THEME_COLOR_IDS)),
    NativeThemeGtkRedirectedEquivalenceTest::ParamInfoToString);
#undef OP

}  // namespace gtk
