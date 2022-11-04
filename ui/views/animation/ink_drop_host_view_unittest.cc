// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_host_view.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_utils.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/test/ink_drop_host_view_test_api.h"
#include "ui/views/animation/test/ink_drop_impl_test_api.h"
#include "ui/views/animation/test/test_ink_drop.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace views::test {
using InkDropMode = InkDropHostTestApi::InkDropMode;

class TestViewWithInkDrop : public View {
 public:
  TestViewWithInkDrop() {
    InkDrop::Install(this, std::make_unique<InkDropHost>(this));
    InkDrop::Get(this)->SetCreateInkDropCallback(base::BindRepeating(
        [](TestViewWithInkDrop* host) -> std::unique_ptr<InkDrop> {
          auto ink_drop = std::make_unique<TestInkDrop>();
          host->last_created_inkdrop_ = ink_drop.get();
          return ink_drop;
        },
        this));
    InkDrop::Get(this)->SetBaseColor(gfx::kPlaceholderColor);
  }

  TestViewWithInkDrop(const TestViewWithInkDrop&) = delete;
  TestViewWithInkDrop& operator=(const TestViewWithInkDrop&) = delete;

  // Expose EventTarget::target_handler() for testing.
  ui::EventHandler* GetTargetHandler() { return target_handler(); }

  TestInkDrop* last_created_inkdrop() const { return last_created_inkdrop_; }

 private:
  raw_ptr<TestInkDrop> last_created_inkdrop_ = nullptr;
};

class InkDropHostViewTest : public testing::Test {
 public:
  InkDropHostViewTest();
  InkDropHostViewTest(const InkDropHostViewTest&) = delete;
  InkDropHostViewTest& operator=(const InkDropHostViewTest&) = delete;
  ~InkDropHostViewTest() override;

 protected:
  // Test target.
  TestViewWithInkDrop host_view_;

  // Provides internal access to |host_view_| test target.
  InkDropHostTestApi test_api_;

  std::unique_ptr<base::AutoReset<gfx::Animation::RichAnimationRenderMode>>
      animation_mode_reset_;

  void MouseEventTriggersInkDropHelper(InkDropMode ink_drop_mode);
};

InkDropHostViewTest::InkDropHostViewTest()
    : test_api_(InkDrop::Get(&host_view_)),
      animation_mode_reset_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
          gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED)) {}

InkDropHostViewTest::~InkDropHostViewTest() = default;

void InkDropHostViewTest::MouseEventTriggersInkDropHelper(
    InkDropMode ink_drop_mode) {
  test_api_.SetInkDropMode(ink_drop_mode);
  host_view_.SetEnabled(true);

  // Call InkDrop::Get(this)->GetInkDrop() to make sure the test
  // CreateInkDrop() is created.
  test_api_.GetInkDrop();
  if (ink_drop_mode != views::InkDropHost::InkDropMode::OFF)
    EXPECT_FALSE(host_view_.last_created_inkdrop()->is_hovered());
  else
    EXPECT_EQ(host_view_.last_created_inkdrop(), nullptr);

  ui::MouseEvent mouse_event(ui::ET_MOUSE_ENTERED, gfx::Point(0, 0),
                             gfx::Point(0, 0), ui::EventTimeForNow(),
                             ui::EF_IS_SYNTHESIZED, 0);

  host_view_.GetTargetHandler()->OnEvent(&mouse_event);

  if (ink_drop_mode != views::InkDropHost::InkDropMode::OFF)
    EXPECT_TRUE(host_view_.last_created_inkdrop()->is_hovered());
  else
    EXPECT_EQ(host_view_.last_created_inkdrop(), nullptr);
}

// Verifies the return value of GetInkDropCenterBasedOnLastEvent() for a null
// Event.
TEST_F(InkDropHostViewTest, GetInkDropCenterBasedOnLastEventForNullEvent) {
  host_view_.SetSize(gfx::Size(20, 20));
  test_api_.AnimateToState(InkDropState::ACTION_PENDING, nullptr);
  EXPECT_EQ(gfx::Point(10, 10),
            InkDrop::Get(&host_view_)->GetInkDropCenterBasedOnLastEvent());
}

// Verifies the return value of GetInkDropCenterBasedOnLastEvent() for a located
// Event.
TEST_F(InkDropHostViewTest, GetInkDropCenterBasedOnLastEventForLocatedEvent) {
  host_view_.SetSize(gfx::Size(20, 20));

  ui::MouseEvent located_event(ui::ET_MOUSE_PRESSED, gfx::Point(5, 6),
                               gfx::Point(5, 6), ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);

  test_api_.AnimateToState(InkDropState::ACTION_PENDING, &located_event);
  EXPECT_EQ(gfx::Point(5, 6),
            InkDrop::Get(&host_view_)->GetInkDropCenterBasedOnLastEvent());
}

TEST_F(InkDropHostViewTest, HasInkDrop) {
  EXPECT_FALSE(test_api_.HasInkDrop());

  test_api_.GetInkDrop();
  EXPECT_TRUE(test_api_.HasInkDrop());

  test_api_.SetInkDropMode(views::InkDropHost::InkDropMode::OFF);
  EXPECT_FALSE(test_api_.HasInkDrop());
}

// Verifies that mouse events trigger ink drops when ink drop mode is ON.
TEST_F(InkDropHostViewTest, MouseEventsTriggerInkDropsWhenInkDropIsOn) {
  MouseEventTriggersInkDropHelper(views::InkDropHost::InkDropMode::ON);
}

// Verifies that mouse events trigger ink drops when ink drop mode is
// ON_NO_GESTURE_HANDLER.
TEST_F(InkDropHostViewTest,
       MouseEventsTriggerInkDropsWhenInkDropIsOnNoGestureHandler) {
  MouseEventTriggersInkDropHelper(
      views::InkDropHost::InkDropMode::ON_NO_GESTURE_HANDLER);
}

// Verifies that mouse events do not trigger ink drops when ink drop mode is
// OFF.
TEST_F(InkDropHostViewTest, MouseEventsDontTriggerInkDropsWhenInkDropIsOff) {
  MouseEventTriggersInkDropHelper(views::InkDropHost::InkDropMode::OFF);
}

// Verifies that ink drops are not shown when the host is disabled.
TEST_F(InkDropHostViewTest,
       GestureEventsDontTriggerInkDropsWhenHostIsDisabled) {
  test_api_.SetInkDropMode(views::InkDropHost::InkDropMode::ON);
  host_view_.SetEnabled(false);

  ui::GestureEvent gesture_event(
      0.f, 0.f, 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::ET_GESTURE_TAP_DOWN));

  host_view_.GetTargetHandler()->OnEvent(&gesture_event);

  EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
            InkDropState::HIDDEN);
}

// Verifies that ink drops are not triggered by gesture events when ink drop
// mode is ON_NO_GESTURE_EVENT or OFF.
TEST_F(InkDropHostViewTest,
       GestureEventsDontTriggerInkDropsWhenInkDropModeIsNotOn) {
  for (auto ink_drop_mode :
       {views::InkDropHost::InkDropMode::ON_NO_GESTURE_HANDLER,
        views::InkDropHost::InkDropMode::OFF}) {
    test_api_.SetInkDropMode(ink_drop_mode);
    ui::GestureEvent gesture_event(
        0.f, 0.f, 0, ui::EventTimeForNow(),
        ui::GestureEventDetails(ui::ET_GESTURE_TAP_DOWN));

    host_view_.GetTargetHandler()->OnEvent(&gesture_event);

    EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
              InkDropState::HIDDEN);
  }
}

#if BUILDFLAG(IS_WIN)
TEST_F(InkDropHostViewTest, NoInkDropOnTouchOrGestureEvents) {
  host_view_.SetSize(gfx::Size(20, 20));

  test_api_.SetInkDropMode(
      views::InkDropHost::InkDropMode::ON_NO_GESTURE_HANDLER);

  // Ensure the target ink drop is in the expected state.
  EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
            InkDropState::HIDDEN);

  ui::TouchEvent touch_event(
      ui::ET_TOUCH_PRESSED, gfx::Point(5, 6), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));

  test_api_.AnimateToState(InkDropState::ACTION_PENDING, &touch_event);
  EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
            InkDropState::HIDDEN);

  test_api_.AnimateToState(InkDropState::ALTERNATE_ACTION_PENDING,
                           &touch_event);
  EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
            InkDropState::HIDDEN);

  ui::GestureEvent gesture_event(5.0f, 6.0f, 0, ui::EventTimeForNow(),
                                 ui::GestureEventDetails(ui::ET_GESTURE_TAP));

  test_api_.AnimateToState(InkDropState::ACTION_PENDING, &gesture_event);
  EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
            InkDropState::HIDDEN);

  test_api_.AnimateToState(InkDropState::ALTERNATE_ACTION_PENDING,
                           &gesture_event);
  EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
            InkDropState::HIDDEN);
}

TEST_F(InkDropHostViewTest, DismissInkDropOnTouchOrGestureEvents) {
  // TODO(bruthig): Re-enable! For some reason these tests fail on some win
  // trunk builds. See crbug.com/731811.
  if (!gfx::Animation::ShouldRenderRichAnimation())
    return;

  host_view_.SetSize(gfx::Size(20, 20));

  test_api_.SetInkDropMode(
      views::InkDropHost::InkDropMode::ON_NO_GESTURE_HANDLER);

  // Ensure the target ink drop is in the expected state.
  EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
            InkDropState::HIDDEN);

  ui::MouseEvent mouse_event(ui::ET_MOUSE_PRESSED, gfx::Point(5, 6),
                             gfx::Point(5, 6), ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);

  test_api_.AnimateToState(InkDropState::ACTION_PENDING, &mouse_event);
  EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
            InkDropState::ACTION_PENDING);

  ui::TouchEvent touch_event(
      ui::ET_TOUCH_PRESSED, gfx::Point(5, 6), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));

  test_api_.AnimateToState(InkDropState::ACTION_TRIGGERED, &touch_event);
  EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
            InkDropState::ACTION_TRIGGERED);
}
#endif

// Verifies that calling OnInkDropHighlightedChanged() triggers a property
// changed notification for the highlighted property.
TEST_F(InkDropHostViewTest, HighlightedChangedFired) {
  bool callback_called = false;
  auto subscription =
      InkDrop::Get(&host_view_)
          ->AddHighlightedChangedCallback(base::BindRepeating(
              [](bool* called) { *called = true; }, &callback_called));
  InkDrop::Get(&host_view_)->OnInkDropHighlightedChanged();
  EXPECT_TRUE(callback_called);
}

// A very basic View that hosts an InkDrop.
class BasicTestViewWithInkDrop : public View {
 public:
  BasicTestViewWithInkDrop() {
    InkDrop::Install(this, std::make_unique<InkDropHost>(this));
    // Call SetBaseColor to avoid hitting a NOTREACHED() for fetching an
    // undefined color.
    InkDrop::Get(this)->SetBaseColor(gfx::kPlaceholderColor);
  }
  BasicTestViewWithInkDrop(const BasicTestViewWithInkDrop&) = delete;
  BasicTestViewWithInkDrop& operator=(const BasicTestViewWithInkDrop&) = delete;
  ~BasicTestViewWithInkDrop() override = default;
};

// Tests the existence of layer clipping or layer masking when certain path
// generators are applied on an InkDropHostView.
class InkDropHostViewClippingTest : public testing::Test {
 public:
  InkDropHostViewClippingTest()
      : host_view_test_api_(InkDrop::Get(&host_view_)) {
    // Set up an InkDropHostView. Clipping is based on the size of the view, so
    // make sure the size is non empty.
    host_view_test_api_.SetInkDropMode(views::InkDropHost::InkDropMode::ON);
    host_view_.SetSize(gfx::Size(20, 20));

    // The root layer of the ink drop is created the first time GetInkDrop is
    // called and then kept alive until the host view is destroyed.
    ink_drop_ =
        static_cast<InkDropImpl*>(InkDrop::Get(&host_view_)->GetInkDrop());
    ink_drop_test_api_ = std::make_unique<test::InkDropImplTestApi>(ink_drop_);
  }
  InkDropHostViewClippingTest(const InkDropHostViewClippingTest&) = delete;
  InkDropHostViewClippingTest& operator=(const InkDropHostViewClippingTest&) =
      delete;
  ~InkDropHostViewClippingTest() override = default;

  ui::Layer* GetRootLayer() { return ink_drop_test_api_->GetRootLayer(); }

 protected:
  // Test target.
  BasicTestViewWithInkDrop host_view_;

  // Provides internal access to |host_view_| test target.
  InkDropHostTestApi host_view_test_api_;

  raw_ptr<InkDropImpl> ink_drop_ = nullptr;

  // Provides internal access to |host_view_|'s ink drop.
  std::unique_ptr<test::InkDropImplTestApi> ink_drop_test_api_;
};

// Tests that by default (no highlight path generator applied), the root layer
// will be masked.
TEST_F(InkDropHostViewClippingTest, DefaultInkDropMasksRootLayer) {
  ink_drop_->SetHovered(true);
  EXPECT_TRUE(GetRootLayer()->layer_mask_layer());
  EXPECT_TRUE(GetRootLayer()->clip_rect().IsEmpty());
}

// Tests that when adding a non empty highlight path generator, the root layer
// is clipped instead of masked.
TEST_F(InkDropHostViewClippingTest,
       HighlightPathGeneratorClipsRootLayerWithoutMask) {
  views::InstallRectHighlightPathGenerator(&host_view_);
  ink_drop_->SetHovered(true);
  EXPECT_FALSE(GetRootLayer()->layer_mask_layer());
  EXPECT_FALSE(GetRootLayer()->clip_rect().IsEmpty());
}

// An empty highlight path generator is used for views who do not want their
// highlight or ripple constrained by their size. Test that the views' ink
// drop root layers have neither a clip or mask.
TEST_F(InkDropHostViewClippingTest,
       EmptyHighlightPathGeneratorUsesNeitherMaskNorClipsRootLayer) {
  views::InstallEmptyHighlightPathGenerator(&host_view_);
  ink_drop_->SetHovered(true);
  EXPECT_FALSE(GetRootLayer()->layer_mask_layer());
  EXPECT_TRUE(GetRootLayer()->clip_rect().IsEmpty());
}

}  // namespace views::test
