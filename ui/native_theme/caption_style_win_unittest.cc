// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/native_theme/caption_style.h"

#include <memory>
#include <string_view>

#include "base/functional/callback_helpers.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "base/win/scoped_com_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/native_theme/caption_style_win.h"

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

class CaptionStyleWinTestBase : public ::testing::Test {
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

class CaptionStyleWinTest
    : public CaptionStyleWinTestBase,
      public ::testing::WithParamInterface<CaptionThemeTestCase> {};

using CaptionStyleWinCacheTest = CaptionStyleWinTestBase;

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

// Verifies that the style read from the OS is reused until the cache is
// invalidated, which is what keeps FromSystemSettings() cheap on the UI
// thread.
TEST_F(CaptionStyleWinCacheTest, StyleIsCachedUntilInvalidated) {
  // Caching is only enabled once something observes OS caption setting
  // changes; NativeThemeWin does this in production.
  base::ScopedClosureRunner runner = ui::EnableCaptionStyleCaching();

  ASSERT_NO_FATAL_FAILURE(SeedCaptionPropertyRegistryValues());
  ASSERT_NO_FATAL_FAILURE(SetCurrentSelectedTheme(kDefaultThemeGuid));

  std::optional<ui::CaptionStyle> caption_style =
      ui::CaptionStyle::FromSystemSettings();
  ASSERT_TRUE(caption_style.has_value());
  ASSERT_EQ(caption_style->text_color.find("!important"), std::string::npos);

  // Selecting a non-default theme makes the style !important, but the cached
  // value is returned until it is invalidated.
  ASSERT_NO_FATAL_FAILURE(SetCurrentSelectedTheme(kCustomThemeGuid));
  std::optional<ui::CaptionStyle> cached_style =
      ui::CaptionStyle::FromSystemSettings();
  ASSERT_TRUE(cached_style.has_value());
  EXPECT_EQ(cached_style->text_color, caption_style->text_color);

  ui::InvalidateCaptionStyleCache();

  std::optional<ui::CaptionStyle> updated_style =
      ui::CaptionStyle::FromSystemSettings();
  ASSERT_TRUE(updated_style.has_value());
  EXPECT_NE(updated_style->text_color.find("!important"), std::string::npos);
}

// Verifies that releasing the runner disables caching and drops any cached
// style.
TEST_F(CaptionStyleWinCacheTest, ReleasingRunnerDisablesCaching) {
  ASSERT_NO_FATAL_FAILURE(SeedCaptionPropertyRegistryValues());
  ASSERT_NO_FATAL_FAILURE(SetCurrentSelectedTheme(kDefaultThemeGuid));

  {
    base::ScopedClosureRunner runner = ui::EnableCaptionStyleCaching();
    std::optional<ui::CaptionStyle> initial_style =
        ui::CaptionStyle::FromSystemSettings();
    ASSERT_TRUE(initial_style.has_value());

    // Selecting a non-default theme while runner is active returns the cached
    // style.
    ASSERT_NO_FATAL_FAILURE(SetCurrentSelectedTheme(kCustomThemeGuid));
    std::optional<ui::CaptionStyle> cached_style =
        ui::CaptionStyle::FromSystemSettings();
    ASSERT_TRUE(cached_style.has_value());
    EXPECT_EQ(cached_style->text_color, initial_style->text_color);
  }

  // Once the runner is released, caching is disabled so the updated theme is
  // re-read from the OS immediately without needing
  // InvalidateCaptionStyleCache().
  std::optional<ui::CaptionStyle> updated_style =
      ui::CaptionStyle::FromSystemSettings();
  ASSERT_TRUE(updated_style.has_value());
  EXPECT_NE(updated_style->text_color.find("!important"), std::string::npos);
}

// Verifies that moving a runner transfers ownership.
TEST_F(CaptionStyleWinCacheTest, MoveRunnerTransfersOwnership) {
  ASSERT_NO_FATAL_FAILURE(SeedCaptionPropertyRegistryValues());
  ASSERT_NO_FATAL_FAILURE(SetCurrentSelectedTheme(kDefaultThemeGuid));

  base::ScopedClosureRunner runner = ui::EnableCaptionStyleCaching();
  std::optional<ui::CaptionStyle> initial_style =
      ui::CaptionStyle::FromSystemSettings();
  ASSERT_TRUE(initial_style.has_value());

  base::ScopedClosureRunner moved_runner = std::move(runner);

  // Caching remains enabled because moved_runner holds ownership.
  ASSERT_NO_FATAL_FAILURE(SetCurrentSelectedTheme(kCustomThemeGuid));
  EXPECT_EQ(ui::CaptionStyle::FromSystemSettings()->text_color,
            initial_style->text_color);

  ui::InvalidateCaptionStyleCache();
  EXPECT_NE(ui::CaptionStyle::FromSystemSettings()->text_color,
            initial_style->text_color);
}

#if GTEST_HAS_DEATH_TEST
// Verifies that at most one runner may exist at a time in a process.
TEST_F(CaptionStyleWinCacheTest, MultipleRunnersNotAllowed) {
  base::ScopedClosureRunner runner = ui::EnableCaptionStyleCaching();
  EXPECT_DEATH_IF_SUPPORTED(
      { base::ScopedClosureRunner r = ui::EnableCaptionStyleCaching(); }, "");
}
#endif

INSTANTIATE_TEST_SUITE_P(
    All,
    CaptionStyleWinTest,
    ::testing::ValuesIn(kCaptionThemeTestCases),
    [](const ::testing::TestParamInfo<CaptionThemeTestCase>& info) {
      return info.param.expect_important ? "NonDefault" : "Default";
    });

}  // namespace ui
