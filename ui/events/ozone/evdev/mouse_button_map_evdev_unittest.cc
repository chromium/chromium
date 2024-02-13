// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/mouse_button_map_evdev.h"

#include <linux/input-event-codes.h>

#include <optional>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ui {
namespace {
constexpr int kDeviceId1 = 1001;
constexpr int kDeviceId2 = 1002;
}  // namespace

TEST(MouseButtonMapTest, SharedDeviceSettingsMapping) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(ash::features::kInputDeviceSettingsSplit);
  ui::MouseButtonMapEvdev mouse_button_map;

  // By default, should be identity map.
  EXPECT_EQ(BTN_LEFT, mouse_button_map.GetMappedButton(kDeviceId1, BTN_LEFT));
  EXPECT_EQ(BTN_LEFT, mouse_button_map.GetMappedButton(kDeviceId2, BTN_LEFT));

  mouse_button_map.SetPrimaryButtonRight(std::nullopt, true);
  EXPECT_EQ(BTN_RIGHT, mouse_button_map.GetMappedButton(kDeviceId1, BTN_LEFT));
  EXPECT_EQ(BTN_RIGHT, mouse_button_map.GetMappedButton(kDeviceId2, BTN_LEFT));

  mouse_button_map.SetPrimaryButtonRight(std::nullopt, false);
  EXPECT_EQ(BTN_LEFT, mouse_button_map.GetMappedButton(kDeviceId1, BTN_LEFT));
  EXPECT_EQ(BTN_LEFT, mouse_button_map.GetMappedButton(kDeviceId2, BTN_LEFT));
}

TEST(MouseButtonMapTest, PerDeviceMapping) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ash::features::kInputDeviceSettingsSplit);
  ui::MouseButtonMapEvdev mouse_button_map;

  // By default, should be identity map.
  EXPECT_EQ(BTN_LEFT, mouse_button_map.GetMappedButton(kDeviceId1, BTN_LEFT));
  EXPECT_EQ(BTN_LEFT, mouse_button_map.GetMappedButton(kDeviceId2, BTN_LEFT));

  mouse_button_map.SetPrimaryButtonRight(kDeviceId1, true);
  EXPECT_EQ(BTN_RIGHT, mouse_button_map.GetMappedButton(kDeviceId1, BTN_LEFT));
  EXPECT_EQ(BTN_LEFT, mouse_button_map.GetMappedButton(kDeviceId2, BTN_LEFT));

  mouse_button_map.SetPrimaryButtonRight(kDeviceId2, true);
  EXPECT_EQ(BTN_RIGHT, mouse_button_map.GetMappedButton(kDeviceId1, BTN_LEFT));
  EXPECT_EQ(BTN_RIGHT, mouse_button_map.GetMappedButton(kDeviceId2, BTN_LEFT));

  mouse_button_map.SetPrimaryButtonRight(kDeviceId2, false);
  EXPECT_EQ(BTN_RIGHT, mouse_button_map.GetMappedButton(kDeviceId1, BTN_LEFT));
  EXPECT_EQ(BTN_LEFT, mouse_button_map.GetMappedButton(kDeviceId2, BTN_LEFT));
}

TEST(MouseButtonMapTest, RemoveDeviceFromSettings) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(ash::features::kInputDeviceSettingsSplit);
  ui::MouseButtonMapEvdev mouse_button_map;

  mouse_button_map.SetPrimaryButtonRight(kDeviceId1, true);
  EXPECT_EQ(BTN_RIGHT, mouse_button_map.GetMappedButton(kDeviceId1, BTN_LEFT));

  // When removed, it should go back to the default.
  mouse_button_map.RemoveDeviceFromSettings(kDeviceId1);
  EXPECT_EQ(BTN_LEFT, mouse_button_map.GetMappedButton(kDeviceId1, BTN_LEFT));
}

}  // namespace ui
