// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/mojom/screen_infos_mojom_traits.h"

#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/mojom/screen_infos.mojom.h"
#include "ui/display/screen_infos.h"

namespace {

using mojo::test::SerializeAndDeserialize;

TEST(StructTraitsTest, ScreenInfosRoundtripValid) {
  display::ScreenInfos i{display::ScreenInfo()};
  display::ScreenInfos o;
  EXPECT_EQ(i.current_display_id, i.screen_infos[0].display_id);
  ASSERT_TRUE(SerializeAndDeserialize<display::mojom::ScreenInfos>(i, o));
  EXPECT_EQ(i, o);
}

TEST(StructTraitsTest, ScreenInfosMustBeNonEmpty) {
  // These fail; screen_infos must contain at least one element.
  display::mojom::ScreenInfosPtr i1 = display::mojom::ScreenInfos::New();
  display::ScreenInfos i2, o;
  ASSERT_FALSE(SerializeAndDeserialize<display::mojom::ScreenInfos>(i1, o));
  EXPECT_DEATH_IF_SUPPORTED(
      display::mojom::ScreenInfos::SerializeAsMessage(&i2), "");
}

TEST(StructTraitsTest, ScreenInfosSelectedDisplayIdMustBePresent) {
  // These fail; current_display_id must match a screen_infos element.
  display::mojom::ScreenInfosPtr i1 = display::mojom::ScreenInfos::New();
  display::ScreenInfos i2, o;
  i1->screen_infos = {display::ScreenInfo()};
  i1->current_display_id = i1->screen_infos[0].display_id + 123;
  i2.screen_infos = {display::ScreenInfo()};
  i2.current_display_id = i2.screen_infos[0].display_id + 123;
  EXPECT_NE(i1->current_display_id, i1->screen_infos[0].display_id);
  EXPECT_NE(i2.current_display_id, i2.screen_infos[0].display_id);
  ASSERT_FALSE(SerializeAndDeserialize<display::mojom::ScreenInfos>(i1, o));
  EXPECT_DEATH_IF_SUPPORTED(
      display::mojom::ScreenInfos::SerializeAsMessage(&i2), "");
}

TEST(StructTraitsTest, ScreenInfosDisplayIdsMustBeUnique) {
  // These fail; screen_infos must use unique display_id values.
  display::mojom::ScreenInfosPtr i1 = display::mojom::ScreenInfos::New();
  display::ScreenInfos i2, o;
  i1->screen_infos = {display::ScreenInfo(), display::ScreenInfo()};
  i2.screen_infos = {display::ScreenInfo(), display::ScreenInfo()};
  EXPECT_EQ(i1->screen_infos[0].display_id, i1->screen_infos[1].display_id);
  EXPECT_EQ(i2.screen_infos[0].display_id, i2.screen_infos[1].display_id);
  ASSERT_FALSE(SerializeAndDeserialize<display::mojom::ScreenInfos>(i1, o));
  EXPECT_DEATH_IF_SUPPORTED(
      display::mojom::ScreenInfos::SerializeAsMessage(&i2), "");
}

}  // namespace
