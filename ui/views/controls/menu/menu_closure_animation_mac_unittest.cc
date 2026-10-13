// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_closure_animation_mac.h"

#include <memory>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/scoped_animation_duration_scale_mode.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/test/menu_test_utils.h"
#include "ui/views/test/views_test_base.h"

TEST(MenuClosureAnimationMacTest, DestructCancelsCleanly) {
  views::test::DisableMenuClosureAnimations();
  base::test::TaskEnvironment environment;

  bool called = false;
  auto animation = std::make_unique<views::MenuClosureAnimationMac>(
      nullptr, nullptr, base::BindLambdaForTesting([&]() { called = true; }));

  animation->Start();
  animation.reset();

  // If the animation callback runs after the animation is destroyed, this line
  // may crash; if not, |called| will get set to true and fail the test below.
  views::test::WaitForMenuClosureAnimation();

  EXPECT_FALSE(called);
}

namespace {

class MenuClosureAnimationMacViewsTest : public views::ViewsTestBase {
 public:
  MenuClosureAnimationMacViewsTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
};

}  // namespace

// Regression test for crbug.com/571434516: destroying the targeted submenu
// while the fade is running must not leave AnimationProgressed() with a
// dangling pointer, and the closure callback must still run.
TEST_F(MenuClosureAnimationMacViewsTest, SubmenuDestroyedDuringFade) {
  // Explicitly enable animations and set normal duration so ticks fire.
  views::MenuClosureAnimationMac::ScopedDisableAnimationsForTesting
      enable_animations(/*disable=*/false);
  gfx::ScopedAnimationDurationScaleMode normal_duration(
      gfx::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  views::test::TestMenuDelegate delegate;
  auto root_item = std::make_unique<views::MenuItemView>(&delegate);
  views::MenuItemView* submenu_item = root_item->AppendSubMenu(1, u"Submenu");
  views::SubmenuView* child_submenu = submenu_item->GetSubmenu();

  bool finished = false;
  auto animation = std::make_unique<views::MenuClosureAnimationMac>(
      nullptr, child_submenu,
      base::BindLambdaForTesting([&]() { finished = true; }));

  animation->Start();
  EXPECT_EQ(child_submenu, animation->menu());

  // Destroy the item that owns the targeted submenu.
  root_item->GetSubmenu()->RemoveChildViewT(submenu_item).reset();
  EXPECT_EQ(nullptr, animation->menu());

  // Step through the fade. AnimationProgressed() would dereference the freed
  // SubmenuView here if it weren't tracked.
  task_environment()->FastForwardBy(base::Milliseconds(300));

  EXPECT_TRUE(finished);
}
