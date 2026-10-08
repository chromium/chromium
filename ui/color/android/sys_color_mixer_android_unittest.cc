// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/color/android/sys_color_mixer_android.h"

#include <algorithm>
#include <set>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/android/android_color_roles.h"
#include "ui/color/color_id.h"

namespace ui {
namespace {

TEST(SysColorMixerAndroidTest, SurfaceContainerRolesAreMapped) {
  constexpr std::pair<AndroidColorRole, ColorId> kExpected[] = {
      {AndroidColorRole::kSurfaceContainerLowest,
       kColorSysSurfaceContainerLowest},
      {AndroidColorRole::kSurfaceContainerLow, kColorSysSurfaceContainerLow},
      {AndroidColorRole::kSurfaceContainer, kColorSysSurfaceContainer},
      {AndroidColorRole::kSurfaceContainerHigh, kColorSysSurfaceContainerHigh},
      {AndroidColorRole::kSurfaceContainerHighest,
       kColorSysSurfaceContainerHighest},
  };
  for (const auto& pair : kExpected) {
    EXPECT_TRUE(
        std::ranges::contains(internal::kAndroidColorRoleToColorId, pair))
        << "Missing mapping for role " << static_cast<int>(pair.first);
  }
}

TEST(SysColorMixerAndroidTest, RolesAreValidAndUnique) {
  std::set<AndroidColorRole> roles;
  for (const auto& [role, id] : internal::kAndroidColorRoleToColorId) {
    EXPECT_GE(static_cast<int>(role), 0);
    EXPECT_LE(role, AndroidColorRole::kMaxValue);
    EXPECT_TRUE(roles.insert(role).second)
        << "Duplicate role " << static_cast<int>(role);
  }
}

TEST(SysColorMixerAndroidTest, ColorIdsAreUnique) {
  std::set<ColorId> ids;
  for (const auto& [role, id] : internal::kAndroidColorRoleToColorId) {
    EXPECT_TRUE(ids.insert(id).second) << "Duplicate ColorId " << id;
  }
}

}  // namespace
}  // namespace ui
