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
#include "ui/decoration/decoration.h"
#include "ui/decoration/shadow.h"
#include "ui/wm/core/shadow_controller_delegate.h"
#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace wm {

namespace {

// The shadow drawing `decoration`.
ui::decoration::Shadow* GetShadow(ui::Decoration* decoration) {
  return decoration->GetSourceAs<ui::decoration::Shadow>();
}

}  // namespace

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
  auto window = std::make_unique<aura::Window>(nullptr);
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  ParentWindow(window.get());

  // The shadow is not created until the Window is shown (some Windows should
  // never get shadows, which is checked when the window first becomes visible).
  EXPECT_FALSE(ShadowController::GetShadowDecorationForWindow(window.get()));
  window->Show();

  const ui::Decoration* shadow_decoration =
      ShadowController::GetShadowDecorationForWindow(window.get());
  ASSERT_TRUE(shadow_decoration);
  EXPECT_TRUE(shadow_decoration->layer()->visible());

  // The shadow should remain visible after window visibility changes.
  window->Hide();
  EXPECT_TRUE(shadow_decoration->layer()->visible());

  // If the shadow is disabled, it should be hidden.
  SetShadowElevation(window.get(), kShadowElevationNone);
  window->Show();
  EXPECT_FALSE(shadow_decoration->layer()->visible());
  SetShadowElevation(window.get(), kShadowElevationInactiveWindow);
  EXPECT_TRUE(shadow_decoration->layer()->visible());

  // The shadow's layer should be a child of the window's layer.
  EXPECT_EQ(window->layer(), shadow_decoration->layer()->parent());
}

// Tests that the window's shadow's bounds are updated correctly.
TEST_F(ShadowControllerTest, ShadowBounds) {
  auto window = std::make_unique<aura::Window>(nullptr);
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  ParentWindow(window.get());
  window->Show();

  const gfx::Rect kOldBounds(20, 30, 400, 300);
  window->SetBounds(kOldBounds);

  // When the shadow is first created, it should use the window's size (but
  // remain at the origin, since it's a child of the window's layer).
  SetShadowElevation(window.get(), kShadowElevationInactiveWindow);
  const ui::Decoration* shadow_decoration =
      ShadowController::GetShadowDecorationForWindow(window.get());
  ASSERT_TRUE(shadow_decoration);
  EXPECT_EQ(gfx::Rect(kOldBounds.size()).ToString(),
            shadow_decoration->content_bounds().ToString());

  // When we change the window's bounds, the shadow's should be updated too.
  gfx::Rect kNewBounds(50, 60, 500, 400);
  window->SetBounds(kNewBounds);
  EXPECT_EQ(gfx::Rect(kNewBounds.size()).ToString(),
            shadow_decoration->content_bounds().ToString());
}

// Tests that the window's shadow's bounds are not updated if not following
// the window bounds.
TEST_F(ShadowControllerTest, ShadowBoundsDetached) {
  const gfx::Rect kInitialBounds(20, 30, 400, 300);
  std::unique_ptr<aura::Window> window = aura::test::CreateTestWindow(
      {.parent = root_window(), .bounds = kInitialBounds});
  window->Show();
  const ui::Decoration* shadow_decoration =
      ShadowController::GetShadowDecorationForWindow(window.get());
  ASSERT_TRUE(shadow_decoration);
  EXPECT_EQ(gfx::Rect(kInitialBounds.size()),
            shadow_decoration->content_bounds());

  // When we change the window's bounds, the shadow's should be updated too.
  const gfx::Rect kBounds1(30, 40, 100, 200);
  window->SetBounds(kBounds1);
  EXPECT_EQ(gfx::Rect(kBounds1.size()), shadow_decoration->content_bounds());

  // Once |kUseWindowBoundsForShadow| is false, the shadow's bounds should no
  // longer follow the window bounds.
  window->SetProperty(aura::client::kUseWindowBoundsForShadow, false);
  gfx::Rect kBounds2(50, 60, 500, 400);
  window->SetBounds(kBounds2);
  EXPECT_EQ(gfx::Rect(kBounds1.size()), shadow_decoration->content_bounds());
}

// Tests that activating a window changes the shadow style.
TEST_F(ShadowControllerTest, ShadowStyle) {
  auto window1 = std::make_unique<aura::Window>(nullptr);
  window1->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window1->Init(ui::LAYER_TEXTURED);
  ParentWindow(window1.get());
  window1->SetBounds(gfx::Rect(10, 20, 300, 400));
  window1->Show();
  ActivateWindow(window1.get());

  // window1 is active, so style should have active appearance.
  ui::Decoration* shadow_decoration1 =
      ShadowController::GetShadowDecorationForWindow(window1.get());
  ASSERT_TRUE(shadow_decoration1);
  EXPECT_EQ(kShadowElevationActiveWindow,
            GetShadow(shadow_decoration1)->elevation());

  // Create another window and activate it.
  auto window2 = std::make_unique<aura::Window>(nullptr);
  window2->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window2->Init(ui::LAYER_TEXTURED);
  ParentWindow(window2.get());
  window2->SetBounds(gfx::Rect(11, 21, 301, 401));
  window2->Show();
  ActivateWindow(window2.get());

  // window1 is now inactive, so shadow should go inactive.
  ui::Decoration* shadow_decoration2 =
      ShadowController::GetShadowDecorationForWindow(window2.get());
  ASSERT_TRUE(shadow_decoration2);
  EXPECT_EQ(kShadowElevationInactiveWindow,
            GetShadow(shadow_decoration1)->elevation());
  EXPECT_EQ(kShadowElevationActiveWindow,
            GetShadow(shadow_decoration2)->elevation());
}

// Tests that shadow gets updated when the window show state changes.
TEST_F(ShadowControllerTest, ShowState) {
  auto window = std::make_unique<aura::Window>(nullptr);
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  ParentWindow(window.get());
  window->Show();

  ui::Decoration* shadow_decoration =
      ShadowController::GetShadowDecorationForWindow(window.get());
  ASSERT_TRUE(shadow_decoration);
  EXPECT_EQ(kShadowElevationInactiveWindow,
            GetShadow(shadow_decoration)->elevation());

  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kMaximized);
  EXPECT_FALSE(shadow_decoration->layer()->visible());

  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kNormal);
  EXPECT_TRUE(shadow_decoration->layer()->visible());

  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kFullscreen);
  EXPECT_FALSE(shadow_decoration->layer()->visible());
}

// Tests that we use smaller shadows for tooltips and menus.
TEST_F(ShadowControllerTest, SmallShadowsForTooltipsAndMenus) {
  auto tooltip_window = std::make_unique<aura::Window>(nullptr);
  tooltip_window->SetType(aura::client::WINDOW_TYPE_TOOLTIP);
  tooltip_window->Init(ui::LAYER_TEXTURED);
  ParentWindow(tooltip_window.get());
  tooltip_window->SetBounds(gfx::Rect(10, 20, 300, 400));
  tooltip_window->Show();

  ui::Decoration* tooltip_shadow_decoration =
      ShadowController::GetShadowDecorationForWindow(tooltip_window.get());
  ASSERT_TRUE(tooltip_shadow_decoration);
  EXPECT_EQ(kShadowElevationMenuOrTooltip,
            GetShadow(tooltip_shadow_decoration)->elevation());

  auto menu_window = std::make_unique<aura::Window>(nullptr);
  menu_window->SetType(aura::client::WINDOW_TYPE_MENU);
  menu_window->Init(ui::LAYER_TEXTURED);
  ParentWindow(menu_window.get());
  menu_window->SetBounds(gfx::Rect(10, 20, 300, 400));
  menu_window->Show();

  ui::Decoration* menu_shadow_decoration =
      ShadowController::GetShadowDecorationForWindow(tooltip_window.get());
  ASSERT_TRUE(menu_shadow_decoration);
  EXPECT_EQ(kShadowElevationMenuOrTooltip,
            GetShadow(menu_shadow_decoration)->elevation());
}

// http://crbug.com/120210 - transient parents of certain types of transients
// should not lose their shadow when they lose activation to the transient.
TEST_F(ShadowControllerTest, TransientParentKeepsActiveShadow) {
  auto window1 = std::make_unique<aura::Window>(nullptr);
  window1->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window1->Init(ui::LAYER_TEXTURED);
  ParentWindow(window1.get());
  window1->SetBounds(gfx::Rect(10, 20, 300, 400));
  window1->Show();
  ActivateWindow(window1.get());

  // window1 is active, so style should have active appearance.
  ui::Decoration* shadow_decoration1 =
      ShadowController::GetShadowDecorationForWindow(window1.get());
  ASSERT_TRUE(shadow_decoration1);
  EXPECT_EQ(kShadowElevationActiveWindow,
            GetShadow(shadow_decoration1)->elevation());

  // Create a window that is transient to window1, and that has the 'hide on
  // deactivate' property set. Upon activation, window1 should still have an
  // active shadow.
  auto window2 = std::make_unique<aura::Window>(nullptr);
  window2->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window2->Init(ui::LAYER_TEXTURED);
  ParentWindow(window2.get());
  window2->SetBounds(gfx::Rect(11, 21, 301, 401));
  AddTransientChild(window1.get(), window2.get());
  SetHideOnDeactivate(window2.get(), true);
  window2->Show();
  ActivateWindow(window2.get());

  // window1 is now inactive, but its shadow should still appear active.
  EXPECT_EQ(kShadowElevationActiveWindow,
            GetShadow(shadow_decoration1)->elevation());
}

// Tests that the shadow color will be updated by setting the shadow colors map.
TEST_F(ShadowControllerTest, SetColorsMapToShadow) {
  auto window = std::make_unique<aura::Window>(nullptr);
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  ParentWindow(window.get());
  window->SetBounds(gfx::Rect(10, 20, 300, 400));
  window->Show();

  ui::Decoration* shadow_decoration =
      ShadowController::GetShadowDecorationForWindow(window.get());
  // Before setting color map, the shadow should has default colors.
  const auto* default_details =
      GetShadow(shadow_decoration)->details_for_testing();
  SkColor default_key_color = SkColorSetA(SK_ColorBLACK, 0x3d);
  SkColor default_ambient_color = SkColorSetA(SK_ColorBLACK, 0x1f);
#if BUILDFLAG(IS_CHROMEOS)
  default_ambient_color = SkColorSetA(SK_ColorBLACK, 0x1a);
#endif
  EXPECT_EQ(default_details->spec[0].color(), default_key_color);
  EXPECT_EQ(default_details->spec[1].color(), default_ambient_color);

  // Change shadow colors map.
  ui::ColorProvider color_provider;
  ui::ColorMixer& mixer = color_provider.AddMixer();
  mixer[ui::kColorShadowValueKeyShadowElevationTwelve] = {SK_ColorYELLOW};
  mixer[ui::kColorShadowValueAmbientShadowElevationTwelve] = {SK_ColorRED};
  mixer[ui::kColorShadowValueKeyShadowElevationTwentyFour] = {SK_ColorGREEN};
  mixer[ui::kColorShadowValueAmbientShadowElevationTwentyFour] = {SK_ColorBLUE};

  GetShadow(shadow_decoration)
      ->SetColorMap(ShadowController::GenerateShadowColorsMap(&color_provider));

  // After setting color map, the shadow colors will be updated.
  const auto* inactive_details =
      GetShadow(shadow_decoration)->details_for_testing();
  EXPECT_EQ(inactive_details->spec[0].color(), SK_ColorYELLOW);
  EXPECT_EQ(inactive_details->spec[1].color(), SK_ColorRED);

  // Activate window will change shadow colors.
  ActivateWindow(window.get());
  const auto* active_details =
      GetShadow(shadow_decoration)->details_for_testing();
  EXPECT_EQ(active_details->spec[0].color(), SK_ColorGREEN);
  EXPECT_EQ(active_details->spec[1].color(), SK_ColorBLUE);
}

namespace {

class TestShadowControllerDelegate : public wm::ShadowControllerDelegate {
 public:
  TestShadowControllerDelegate() = default;

  TestShadowControllerDelegate(const TestShadowControllerDelegate&) = delete;
  TestShadowControllerDelegate& operator=(const TestShadowControllerDelegate&) =
      delete;

  ~TestShadowControllerDelegate() override = default;

  bool ShouldObserveWindow(const aura::Window* window) override {
    return window->GetType() != aura::client::WINDOW_TYPE_CONTROL;
  }

  bool ShouldShowShadowForWindow(const aura::Window* window) override {
    return window->parent();
  }

  bool ShouldUpdateShadowOnWindowPropertyChange(const aura::Window* window,
                                                const void* key,
                                                intptr_t old) override {
    return false;
  }

  void ApplyColorThemeToWindowShadow(aura::Window* window) override {}

  bool ShouldRoundShadowForWindow(const aura::Window* window) override {
    return true;
  }
};

}  // namespace

TEST_F(ShadowControllerTest, UpdateShadowWhenAddedToParent) {
  InstallShadowController(std::make_unique<TestShadowControllerDelegate>());
  {
    auto window = std::make_unique<aura::Window>(nullptr);
    window->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    window->SetBounds(gfx::Rect(10, 20, 300, 400));
    window->Show();
    EXPECT_FALSE(ShadowController::GetShadowDecorationForWindow(window.get()));

    ParentWindow(window.get());

    ASSERT_TRUE(ShadowController::GetShadowDecorationForWindow(window.get()));
    EXPECT_TRUE(ShadowController::GetShadowDecorationForWindow(window.get())
                    ->layer()
                    ->visible());
  }
  {
    // The creation of shadow for TYPE_CONTROL is blocked by the delegate.
    auto embedded = std::make_unique<aura::Window>(nullptr);
    embedded->SetType(aura::client::WINDOW_TYPE_CONTROL);
    embedded->Init(ui::LAYER_TEXTURED);
    embedded->SetBounds(gfx::Rect(10, 20, 300, 400));
    embedded->Show();
    EXPECT_FALSE(
        ShadowController::GetShadowDecorationForWindow(embedded.get()));

    ParentWindow(embedded.get());

    ASSERT_FALSE(
        ShadowController::GetShadowDecorationForWindow(embedded.get()));
  }
}

}  // namespace wm
