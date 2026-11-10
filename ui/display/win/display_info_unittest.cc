// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/display_info.h"

#include <windows.h>

#include <optional>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/win/display_config_helper.h"
#include "ui/display/win/test/screen_util_win.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace display::win {
namespace {

DISPLAYCONFIG_PATH_INFO g_display_config_path = {};
int g_display_config_query_count = 0;

std::optional<DISPLAYCONFIG_PATH_INFO> GetFakeDisplayConfigPathInfo(
    const MONITORINFOEX&) {
  ++g_display_config_query_count;
  return g_display_config_path;
}

internal::DisplayInfo CreateDisplayInfoWithConfigPath(
    const MONITORINFOEX& monitor_info,
    const std::optional<DISPLAYCONFIG_PATH_INFO>& config_path) {
  return internal::DisplayInfo(
      std::nullopt, monitor_info, config_path,
      /*device_scale_factor=*/1.0f, /*text_scale_multiplier=*/1.0f,
      Display::kDefaultBitsPerPixel, /*sdr_white_level=*/200.0f,
      Display::ROTATE_0,
      /*display_frequency=*/60.0f, gfx::Vector2dF(),
      DISPLAYCONFIG_OUTPUT_TECHNOLOGY_OTHER, std::string());
}

}  // namespace

// Monitor information is read from Windows, then used to create a
// DisplayInfo object. Windows may change the monitor setup between these
// steps. This test checks that the object's ID still comes from the saved
// information, without asking Windows again.
TEST(DisplayInfoTest, DoesNotRequeryAfterConfigurationChanges) {
  constexpr UINT32 kCollectedTargetId = 1;
  constexpr UINT32 kNewTargetId = 2;
  g_display_config_path = {};
  g_display_config_path.targetInfo.adapterId = {1, 0};
  g_display_config_path.targetInfo.id = kCollectedTargetId;
  auto scoped_override =
      SetDisplayConfigPathInfoGetterForTesting(&GetFakeDisplayConfigPathInfo);
  const MONITORINFOEX monitor_info = win::test::CreateMonitorInfo(
      gfx::Rect(1920, 1080), gfx::Rect(1920, 1040), L"test_monitor");
  const auto collected_path = GetDisplayConfigPathInfo(monitor_info);
  ASSERT_TRUE(collected_path);
  const auto before =
      CreateDisplayInfoWithConfigPath(monitor_info, collected_path);

  // Pretend Windows now reports a different monitor setup.
  g_display_config_path.targetInfo.id = kNewTargetId;
  g_display_config_query_count = 0;
  const auto after =
      CreateDisplayInfoWithConfigPath(monitor_info, collected_path);
  EXPECT_EQ(before.id(), after.id());
  // Missing saved display details must not make the object ask Windows again.
  CreateDisplayInfoWithConfigPath(monitor_info, std::nullopt);
  EXPECT_EQ(0, g_display_config_query_count);

  // Requesting the new monitor information really does produce a different ID.
  const auto latest_path = GetDisplayConfigPathInfo(monitor_info);
  ASSERT_TRUE(latest_path);
  EXPECT_NE(before.id(),
            CreateDisplayInfoWithConfigPath(monitor_info, latest_path).id());
}

}  // namespace display::win
