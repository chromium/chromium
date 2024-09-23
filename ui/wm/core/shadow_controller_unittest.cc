// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/shadow_controller.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/color/color_id.h"
#include "ui/color/color_mixer.h"
#include "ui/color/color_provider.h"
#include "ui/color/color_recipe.h"
#include "ui/compositor/layer.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/wm/core/shadow_controller_delegate.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace wm {

class ShadowControllerTest : public aura::test::AuraTestBase {
 public:
  ShadowControllerTest() {}

  ShadowControllerTest(const ShadowControllerTest&) = delete;
  ShadowControllerTest& operator=(const ShadowControllerTest&) = delete;

  ~ShadowControllerTest() override {}

  void SetUp() override {
    AuraTestBase::SetUp();
    InstallShadowController(nullptr);
  }
  void TearDown() override {
    shadow_controller_.reset();
    AuraTestBase::TearDown();
  }

 protected:
  ShadowController* shadow_controller() { return shadow_controller_.get(); }

  void ActivateWindow(aura::Window* window) {
    DCHECK(window);
    DCHECK(window->GetRootWindow());
    GetActivationClient(window->GetRootWindow())->ActivateWindow(window);
  }

  void InstallShadowController(
      std::unique_ptr<ShadowControllerDelegate> delegate) {
    shadow_controller_ = std::make_unique<ShadowController>(
        GetActivationClient(root_window()), std::move(delegate));
  }

 private:
  std::unique_ptr<ShadowController> shadow_controller_;
};

// Tests that various methods in Window update the Shadow object as expected.
TEST_F(ShadowControllerTest, Shadow) {
  std::unique_ptr<aura::Window> window(new aura::Window(NULL));
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  ParentWindow(window.get());

  // The shadow is not created until the Window is shown (some Windows should
  // never get shadows, which is checked when the window first becomes visible).
  EXPECT_FALSE(ShadowController::GetShadowForWindow(window.get()));
  window->Show();

  const ui::Shadow* shadow = ShadowController::GetShadowForWindow(window.get());
  ASSERT_TRUE(shadow != NULL);
  EXPECT_TRUE(shadow->layer()->visible());

  // The shadow should remain visible after window visibility changes.
  window->Hide();
  EXPECT_TRUE(shadow->layer()->visible());

  // If the shadow is disabled, it should be hidden.
  SetShadowElevation(window.get(), kShadowElevationNone);
  window->Show();
  EXPECT_FALSE(shadow->layer()->visible());
  SetShadowElevation(window.get(), kShadowElevationInactiveWindow);
  EXPECT_TRUE(shadow->layer()->visible());

  // The shadow's layer should be a child of the window's layer.
  EXPECT_EQ(window->layer(), shadow->layer()->parent());
}

// Tests that the window's shadow's bounds are updated correctly.
TEST_F(ShadowControllerTest, ShadowBounds) {
  std::unique_ptr<aura::Window> window(new aura::Window(NULL));
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  ParentWindow(window.get());
  window->Show();

  const gfx::Rect kOldBounds(20, 30, 400, 300);
  window->SetBounds(kOldBounds);

  // When the shadow is first created, it should use the window's size (but
  // remain at the origin, since it's a child of the window's layer).
  SetShadowElevation(window.get(), kShadowElevationInactiveWindow);
  const ui::Shadow* shadow = ShadowController::GetShadowForWindow(window.get());
  ASSERT_TRUE(shadow != NULL);
  EXPECT_EQ(gfx::Rect(kOldBounds.size()).ToString(),
            shadow->content_bounds().ToString());

  // When we change the window's bounds, the shadow's should be updated too.
  gfx::Rect kNewBounds(50, 60, 500, 400);
  window->SetBounds(kNewBounds);
  EXPECT_EQ(gfx::Rect(kNewBounds.size()).ToString(),
            shadow->content_bounds().ToString());
}

// Tests that the window's shadow's bounds are not updated if not following
// the window bounds.
TEST_F(ShadowControllerTest, ShadowBoundsDetached) {
  const gfx::Rect kInitialBounds(20, 30, 400, 300);
  std::unique_ptr<aura::Window> window(
      aura::test::CreateTestWindowWithBounds(kInitialBounds, root_window()));
  window->Show();
  const ui::Shadow* shadow = ShadowController::GetShadowForWindow(window.get());
  ASSERT_TRUE(shadow);
  EXPECT_EQ(gfx::Rect(kInitialBounds.size()), shadow->content_bounds());

  // When we change the window's bounds, the shadow's should be updated too.
  const gfx::Rect kBounds1(30, 40, 100, 200);
  window->SetBounds(kBounds1);
  EXPECT_EQ(gfx::Rect(kBounds1.size()), shadow->content_bounds());

  // Once |kUseWindowBoundsForShadow| is false, the shadow's bounds should no
  // longer follow the window bounds.
  window->SetProperty(aura::client::kUseWindowBoundsForShadow, false);
  gfx::Rect kBounds2(50, 60, 500, 400);
  window->SetBounds(kBounds2);
  EXPECT_EQ(gfx::Rect(kBounds1.size()), shadow->content_bounds());
}

// Tests that activating a window changes the shadow style.
TEST_F(ShadowControllerTest, ShadowStyle) {
  std::unique_ptr<aura::Window> window1(new aura::Window(NULL));
  window1->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window1->Init(ui::LAYER_TEXTURED);
  ParentWindow(window1.get());
  window1->SetBounds(gfx::Rect(10, 20, 300, 400));
  window1->Show();
  ActivateWindow(window1.get());

  // window1 is active, so style should have active appearance.
  ui::Shadow* shadow1 = ShadowController::GetShadowForWindow(window1.get());
  ASSERT_TRUE(shadow1 != NULL);
  EXPECT_EQ(kShadowElevationActiveWindow, shadow1->desired_elevation());

  // Create another window and activate it.
  std::unique_ptr<aura::Window> window2(new aura::Window(NULL));
  window2->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window2->Init(ui::LAYER_TEXTURED);
  ParentWindow(window2.get());
  window2->SetBounds(gfx::Rect(11, 21, 301, 401));
  window2->Show();
  ActivateWindow(window2.get());

  // window1 is now inactive, so shadow should go inactive.
  ui::Shadow* shadow2 = ShadowController::GetShadowForWindow(window2.get());
  ASSERT_TRUE(shadow2 != NULL);
  EXPECT_EQ(kShadowElevationInactiveWindow, shadow1->desired_elevation());
  EXPECT_EQ(kShadowElevationActiveWindow, shadow2->desired_elevation());
}

// Tests that shadow gets updated when the window show state changes.
TEST_F(ShadowControllerTest, ShowState) {
  std::unique_ptr<aura::Window> window(new aura::Window(NULL));
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  ParentWindow(window.get());
  window->Show();

  ui::Shadow* shadow = ShadowController::GetShadowForWindow(window.get());
  ASSERT_TRUE(shadow != NULL);
  EXPECT_EQ(kShadowElevationInactiveWindow, shadow->desired_elevation());

  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kMaximized);
  EXPECT_FALSE(shadow->layer()->visible());

  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kNormal);
  EXPECT_TRUE(shadow->layer()->visible());

  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kFullscreen);
  EXPECT_FALSE(shadow->layer()->visible());
}

// Tests that we use smaller shadows for tooltips and menus.
TEST_F(ShadowControllerTest, SmallShadowsForTooltipsAndMenus) {
  std::unique_ptr<aura::Window> tooltip_window(new aura::Window(NULL));
  tooltip_window->SetType(aura::client::WINDOW_TYPE_TOOLTIP);
  tooltip_window->Init(ui::LAYER_TEXTURED);
  ParentWindow(tooltip_window.get());
  tooltip_window->SetBounds(gfx::Rect(10, 20, 300, 400));
  tooltip_window->Show();

  ui::Shadow* tooltip_shadow =
      ShadowController::GetShadowForWindow(tooltip_window.get());
  ASSERT_TRUE(tooltip_shadow != NULL);
  EXPECT_EQ(kShadowElevationMenuOrTooltip, tooltip_shadow->desired_elevation());

  std::unique_ptr<aura::Window> menu_window(new aura::Window(NULL));
  menu_window->SetType(aura::client::WINDOW_TYPE_MENU);
  menu_window->Init(ui::LAYER_TEXTURED);
  ParentWindow(menu_window.get());
  menu_window->SetBounds(gfx::Rect(10, 20, 300, 400));
  menu_window->Show();

  ui::Shadow* menu_shadow =
      ShadowController::GetShadowForWindow(tooltip_window.get());
  ASSERT_TRUE(menu_shadow != NULL);
  EXPECT_EQ(kShadowElevationMenuOrTooltip, menu_shadow->desired_elevation());
}

// http://crbug.com/120210 - transient parents of certain types of transients
// should not lose their shadow when they lose activation to the transient.
TEST_F(ShadowControllerTest, TransientParentKeepsActiveShadow) {
  std::unique_ptr<aura::Window> window1(new aura::Window(NULL));
  window1->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window1->Init(ui::LAYER_TEXTURED);
  ParentWindow(window1.get());
  window1->SetBounds(gfx::Rect(10, 20, 300, 400));
  window1->Show();
  ActivateWindow(window1.get());

  // window1 is active, so style should have active appearance.
  ui::Shadow* shadow1 = ShadowController::GetShadowForWindow(window1.get());
  ASSERT_TRUE(shadow1 != NULL);
  EXPECT_EQ(kShadowElevationActiveWindow, shadow1->desired_elevation());

  // Create a window that is transient to window1, and that has the 'hide on
  // deactivate' property set. Upon activation, window1 should still have an
  // active shadow.
  std::unique_ptr<aura::Window> window2(new aura::Window(NULL));
  window2->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window2->Init(ui::LAYER_TEXTURED);
  ParentWindow(window2.get());
  window2->SetBounds(gfx::Rect(11, 21, 301, 401));
  AddTransientChild(window1.get(), window2.get());
  SetHideOnDeactivate(window2.get(), true);
  window2->Show();
  ActivateWindow(window2.get());

  // window1 is now inactive, but its shadow should still appear active.
  EXPECT_EQ(kShadowElevationActiveWindow, shadow1->desired_elevation());
}

// Tests that the shadow color will be updated by setting the shadow colors map.
TEST_F(ShadowControllerTest, SetColorsMapToShadow) {
  std::unique_ptr<aura::Window> window(new aura::Window(nullptr));
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  ParentWindow(window.get());
  window->SetBounds(gfx::Rect(10, 20, 300, 400));
  window->Show();

  ui::Shadow* shadow = ShadowController::GetShadowForWindow(window.get());
  // Before setting color map, the shadow should has default colors.
  const auto* default_details = shadow->details_for_testing();
  SkColor default_key_color = SkColorSetA(SK_ColorBLACK, 0x3d);
  SkColor default_ambient_color = SkColorSetA(SK_ColorBLACK, 0x1f);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  default_ambient_color = SkColorSetA(SK_ColorBLACK, 0x1a);
#endif
  EXPECT_EQ(default_details->values[0].color(), default_key_color);
  EXPECT_EQ(default_details->values[1].color(), default_ambient_color);

  // Change shadow colors map.
  ui::ColorProvider color_provider;
  ui::ColorMixer& mixer = color_provider.AddMixer();
  mixer[ui::kColorShadowValueKeyShadowElevationTwelve] = {SK_ColorYELLOW};
  mixer[ui::kColorShadowValueAmbientShadowElevationTwelve] = {SK_ColorRED};
  mixer[ui::kColorShadowValueKeyShadowElevationTwentyFour] = {SK_ColorGREEN};
  mixer[ui::kColorShadowValueAmbientShadowElevationTwentyFour] = {SK_ColorBLUE};

  shadow->SetElevationToColorsMap(
      ShadowController::GenerateShadowColorsMap(&color_provider));

  // After setting color map, the shadow colors will be updated.
  const auto* inactive_details = shadow->details_for_testing();
  EXPECT_EQ(inactive_details->values[0].color(), SK_ColorYELLOW);
  EXPECT_EQ(inactive_details->values[1].color(), SK_ColorRED);

  // Activate window will change shadow colors.
  ActivateWindow(window.get());
  const auto* active_details = shadow->details_for_testing();
  EXPECT_EQ(active_details->values[0].color(), SK_ColorGREEN);
  EXPECT_EQ(active_details->values[1].color(), SK_ColorBLUE);
}

namespace {

class TestShadowControllerDelegate : public wm::ShadowControllerDelegate {
 public:
  TestShadowControllerDelegate() = default;

  TestShadowControllerDelegate(const TestShadowControllerDelegate&) = delete;
  TestShadowControllerDelegate& operator=(const TestShadowControllerDelegate&) =
      delete;

  ~TestShadowControllerDelegate() override = default;

  bool ShouldShowShadowForWindow(const aura::Window* window) override {
    return window->parent();
  }

  bool ShouldUpdateShadowOnWindowPropertyChange(const aura::Window* window,
                                                const void* key,
                                                intptr_t old) override {
    return false;
  }

  void ApplyColorThemeToWindowShadow(aura::Window* window) override {}
};

}  // namespace

TEST_F(ShadowControllerTest, UpdateShadowWhenAddedToParent) {
  InstallShadowController(std::make_unique<TestShadowControllerDelegate>());
  std::unique_ptr<aura::Window> window1(new aura::Window(nullptr));
  window1->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window1->Init(ui::LAYER_TEXTURED);
  window1->SetBounds(gfx::Rect(10, 20, 300, 400));
  window1->Show();
  EXPECT_FALSE(ShadowController::GetShadowForWindow(window1.get()));

  ParentWindow(window1.get());

  ASSERT_TRUE(ShadowController::GetShadowForWindow(window1.get()));
  EXPECT_TRUE(
      ShadowController::GetShadowForWindow(window1.get())->layer()->visible());
}

}  // namespace wm
