// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gtk/nav_button_provider_gtk.h"

#include <glib-object.h>
#include <glib.h>

#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/gtk/gtk_compat.h"
#include "ui/gtk/gtk_util.h"
#include "ui/linux/nav_button_provider.h"

namespace gtk {

namespace {

using DisplayType = ui::NavButtonProvider::FrameButtonDisplayType;
using ButtonState = ui::NavButtonProvider::ButtonState;

// Writes a custom icon theme into `temp_dir`. `window-minimize-symbolic`,
// `window-maximize-symbolic`, and `window-restore-symbolic` live in a
// `Size=16, Type=Scalable` directory (which takes the size-probe shortcut),
// while `window-close-symbolic` lives in a `Size=32, Type=Fixed` directory
// (which must still be decoded to learn its 32px size).
void WriteTestIconTheme(const base::FilePath& temp_dir) {
  if (GtkCheckVersion(4)) {
    return;
  }
  gchar* theme_name_ptr = nullptr;
  g_object_get(GetDefaultGtkSettings(), "gtk-icon-theme-name", &theme_name_ptr,
               nullptr);
  std::string theme_name =
      (theme_name_ptr && *theme_name_ptr) ? theme_name_ptr : "Adwaita";
  g_free(theme_name_ptr);

  const base::FilePath theme_dir = temp_dir.AppendASCII(theme_name);
  const base::FilePath scalable_dir = theme_dir.AppendASCII("scalable");
  const base::FilePath fixed_dir = theme_dir.AppendASCII("fixed");
  ASSERT_TRUE(base::CreateDirectory(scalable_dir));
  ASSERT_TRUE(base::CreateDirectory(fixed_dir));

  constexpr std::string_view kIndexTheme =
      "[Icon Theme]\n"
      "Name=Test\n"
      "Directories=scalable,fixed\n"
      "\n"
      "[scalable]\n"
      "Size=16\n"
      "Type=Scalable\n"
      "MinSize=8\n"
      "MaxSize=512\n"
      "\n"
      "[fixed]\n"
      "Size=32\n"
      "Type=Fixed\n";
  ASSERT_TRUE(
      base::WriteFile(theme_dir.AppendASCII("index.theme"), kIndexTheme));

  constexpr std::string_view kSvg16 =
      "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"16\" height=\"16\">"
      "<rect x=\"2\" y=\"2\" width=\"12\" height=\"12\" class=\"fg\" "
      "fill=\"#bebebe\"/></svg>";
  for (const char* name :
       {"window-minimize-symbolic.svg", "window-maximize-symbolic.svg",
        "window-restore-symbolic.svg"}) {
    ASSERT_TRUE(base::WriteFile(scalable_dir.AppendASCII(name), kSvg16));
  }

  constexpr std::string_view kSvg32 =
      "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"32\" height=\"32\">"
      "<rect x=\"4\" y=\"4\" width=\"24\" height=\"24\" class=\"fg\" "
      "fill=\"#bebebe\"/></svg>";
  ASSERT_TRUE(base::WriteFile(
      fixed_dir.AppendASCII("window-close-symbolic.svg"), kSvg32));
}

// Everything that `NavButtonProviderGtk` reports to the frame after
// `RedrawImages()`, with the button images rasterized at 1x.
struct ProviderOutput {
  gfx::Insets top_area_spacing;
  int inter_button_spacing = 0;
  int nav_button_height = 0;
  std::map<DisplayType, gfx::Insets> margins;
  std::map<std::pair<DisplayType, ButtonState>, SkBitmap> images;
};

ProviderOutput RedrawAndCollect(int top_area_height,
                                bool maximized,
                                bool active) {
  NavButtonProviderGtk provider(ui::FrameType::kBrowser);
  provider.RedrawImages(top_area_height, maximized, active);
  NavButtonProviderGtk::WaitForPrefetchForTesting();

  ProviderOutput output;
  output.top_area_spacing = provider.GetTopAreaSpacing();
  output.inter_button_spacing = provider.GetInterNavButtonSpacing();
  output.nav_button_height = provider.GetNavButtonHeight(maximized);
  for (DisplayType type :
       {DisplayType::kMinimize,
        maximized ? DisplayType::kRestore : DisplayType::kMaximize,
        DisplayType::kClose}) {
    output.margins[type] = provider.GetNavButtonMargin(type);
    for (ButtonState state : {ButtonState::kNormal, ButtonState::kHovered,
                              ButtonState::kPressed, ButtonState::kDisabled}) {
      output.images[{type, state}] =
          provider.GetImage(type, state).GetRepresentation(1.0f).GetBitmap();
    }
  }
  return output;
}

void ExpectSameOutput(const ProviderOutput& expected,
                      const ProviderOutput& actual) {
  EXPECT_EQ(expected.top_area_spacing, actual.top_area_spacing);
  EXPECT_EQ(expected.inter_button_spacing, actual.inter_button_spacing);
  EXPECT_EQ(expected.nav_button_height, actual.nav_button_height);
  EXPECT_EQ(expected.margins, actual.margins);
  ASSERT_EQ(expected.images.size(), actual.images.size());
  for (const auto& [key, bitmap] : expected.images) {
    const SkBitmap& actual_bitmap = actual.images.at(key);
    EXPECT_EQ(bitmap.width(), actual_bitmap.width());
    EXPECT_EQ(bitmap.height(), actual_bitmap.height());
    EXPECT_TRUE(gfx::test::AreBitmapsEqual(bitmap, actual_bitmap));
  }
}

// Skipping the size-probe decode and prefetching the icons must not change the
// button sizes, margins or pixels that the frame gets.
TEST(NavButtonProviderGtkTest, PrefetchDoesNotChangeOutput) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ASSERT_NO_FATAL_FAILURE(WriteTestIconTheme(temp_dir.GetPath()));
  base::ScopedClosureRunner restore_search_path =
      PrependIconThemeSearchPathForTesting(temp_dir.GetPath());

  struct Params {
    int top_area_height;
    bool maximized;
    bool active;
  };
  const std::vector<Params> params = {
      {/*top_area_height=*/100, /*maximized=*/false, /*active=*/true},
      {/*top_area_height=*/100, /*maximized=*/false, /*active=*/false},
      {/*top_area_height=*/100, /*maximized=*/true, /*active=*/true},
      // Too short for the buttons, so they are scaled down.
      {/*top_area_height=*/20, /*maximized=*/false, /*active=*/true},
  };

  std::vector<ProviderOutput> expected;
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(kGtkPrefetchNavButtonIcons);
    for (const auto& p : params) {
      expected.push_back(
          RedrawAndCollect(p.top_area_height, p.maximized, p.active));
    }
  }

  if (!GtkCheckVersion(4)) {
    // Verify the custom theme is active: `window-close-symbolic` comes from the
    // 32px Fixed directory, so its button is larger than
    // `window-minimize-symbolic` from the 16px Scalable directory.
    EXPECT_GE(expected[0]
                  .images.at({DisplayType::kClose, ButtonState::kNormal})
                  .width(),
              32);
    EXPECT_LT(expected[0]
                  .images.at({DisplayType::kMinimize, ButtonState::kNormal})
                  .width(),
              expected[0]
                  .images.at({DisplayType::kClose, ButtonState::kNormal})
                  .width());
  }

  base::test::ScopedFeatureList feature_list(kGtkPrefetchNavButtonIcons);
  for (size_t i = 0; i < params.size(); ++i) {
    SCOPED_TRACE(i);
    ExpectSameOutput(expected[i],
                     RedrawAndCollect(params[i].top_area_height,
                                      params[i].maximized, params[i].active));
  }
}

}  // namespace

}  // namespace gtk
