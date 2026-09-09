// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/caption_style.h"

#include <string_view>

#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "base/win/scoped_com_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {

namespace {

constexpr wchar_t kCaptionRegPath[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\ClosedCaptioning";

// GUID of the built-in Windows "Default" closed caption theme.
constexpr wchar_t kDefaultThemeGuid[] =
    L"{642F4BD2-475F-4802-9B13-95261896CB1C}";

// GUID of a non-default built-in Windows caption theme (Yellow on Blue).
constexpr wchar_t kCustomThemeGuid[] =
    L"{DF834234-A0EF-4E2A-BB87-C40E5D1CFC8C}";

// Seeds the per-property registry values that the Windows.Media.
// ClosedCaptioning runtime API reads. Each ClosedCaption* enum uses 0 to mean
// "Default", so any non-zero DWORD here yields a non-Default enum value
// (e.g. White, Black, Tahoma, 100%). These specific numbers are not meaningful
// — the test only needs the API to report non-Default for each property.
void SeedCaptionPropertyRegistryValues() {
  base::win::RegKey key;
  ASSERT_EQ(key.Create(HKEY_CURRENT_USER, kCaptionRegPath, KEY_WRITE),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(L"CaptionColor", static_cast<DWORD>(1)),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(L"CaptionOpacity", static_cast<DWORD>(1)),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(L"CaptionSize", static_cast<DWORD>(2)),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(L"CaptionFontStyle", static_cast<DWORD>(4)),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(L"CaptionEdgeEffect", static_cast<DWORD>(1)),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(L"BackgroundColor", static_cast<DWORD>(2)),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(L"BackgroundOpacity", static_cast<DWORD>(2)),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(L"RegionColor", static_cast<DWORD>(2)),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(L"RegionOpacity", static_cast<DWORD>(4)),
            ERROR_SUCCESS);
}

void SetCurrentSelectedTheme(const wchar_t* guid) {
  base::win::RegKey key;
  ASSERT_EQ(key.Create(HKEY_CURRENT_USER, kCaptionRegPath, KEY_WRITE),
            ERROR_SUCCESS);
  ASSERT_EQ(key.WriteValue(L"CurrentSelectedTheme", guid), ERROR_SUCCESS);
}

struct CaptionThemeTestCase {
  const wchar_t* guid;
  bool expect_important;
};

constexpr CaptionThemeTestCase kCaptionThemeTestCases[] = {
    // The Default theme should allow page styles to override caption styles.
    {kDefaultThemeGuid, false},
    // A non-Default theme is a user preference that should override page
    // styles with !important.
    {kCustomThemeGuid, true},
};

class CaptionStyleWinTest
    : public ::testing::TestWithParam<CaptionThemeTestCase> {
 protected:
  void SetUp() override {
    // Redirect HKCU to a scratch hive so the test never touches the real user
    // registry and is parallel-safe. The Windows.Media.ClosedCaptioning WinRT
    // API runs in-process and reads HKCU directly, so this redirect also
    // controls what the API observes.
    ASSERT_NO_FATAL_FAILURE(
        registry_override_.OverrideRegistry(HKEY_CURRENT_USER));
  }

 private:
  registry_util::RegistryOverrideManager registry_override_;
  base::win::ScopedCOMInitializer com_initializer_;
};

}  // namespace

TEST_P(CaptionStyleWinTest, TestWinCaptionStyle) {
  const CaptionThemeTestCase& test_case = GetParam();

  ASSERT_NO_FATAL_FAILURE(SeedCaptionPropertyRegistryValues());
  ASSERT_NO_FATAL_FAILURE(SetCurrentSelectedTheme(test_case.guid));

  std::optional<ui::CaptionStyle> caption_style =
      ui::CaptionStyle::FromSystemSettings();
  ASSERT_TRUE(caption_style.has_value());

  auto check_property = [&test_case](std::string_view property_name,
                                     std::string_view value) {
    SCOPED_TRACE(property_name);
    EXPECT_FALSE(value.empty());
    EXPECT_EQ(value.find("!important") != std::string_view::npos,
              test_case.expect_important)
        << "Value: " << value;
  };

  check_property("background_color", caption_style->background_color);
  check_property("font_family", caption_style->font_family);
  check_property("font_variant", caption_style->font_variant);
  check_property("text_color", caption_style->text_color);
  check_property("text_shadow", caption_style->text_shadow);
  check_property("text_size", caption_style->text_size);
  check_property("window_color", caption_style->window_color);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    CaptionStyleWinTest,
    ::testing::ValuesIn(kCaptionThemeTestCases),
    [](const ::testing::TestParamInfo<CaptionThemeTestCase>& info) {
      return info.param.expect_important ? "NonDefault" : "Default";
    });

}  // namespace ui
