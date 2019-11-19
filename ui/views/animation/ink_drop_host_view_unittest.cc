// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_host_view.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/animation/animation.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/animation/test/ink_drop_host_view_test_api.h"
#include "ui/views/animation/test/test_ink_drop.h"

namespace views {
namespace test {
using InkDropMode = InkDropHostViewTestApi::InkDropMode;

class TestInkDropHostView : public InkDropHostView {
 public:
  TestInkDropHostView() = default;

  // Accessors to InkDropHostView internals.
  ui::EventHandler* GetTargetHandler() { return target_handler(); }

  int on_ink_drop_created_count() const { return on_ink_drop_created_count_; }

  TestInkDrop* last_created_inkdrop() const { return last_created_inkdrop_; }

 protected:
  SkColor GetInkDropBaseColor() const override {
    return gfx::kPlaceholderColor;
  }

  // InkDropHostView:
  void OnInkDropCreated() override { ++on_ink_drop_created_count_; }

  std::unique_ptr<InkDrop> CreateInkDrop() override {
    last_created_inkdrop_ = new TestInkDrop();
    return base::WrapUnique(last_created_inkdrop_);
  }

 private:
  int on_ink_drop_created_count_ = 0;
  TestInkDrop* last_created_inkdrop_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TestInkDropHostView);
};

class InkDropHostViewTest : public testing::Test {
 public:
  InkDropHostViewTest();
  ~InkDropHostViewTest() override;

 protected:
  // Test target.
  TestInkDropHostView host_view_;

  // Provides internal access to |host_view_| test target.
  InkDropHostViewTestApi test_api_;

  std::unique_ptr<base::AutoReset<gfx::Animation::RichAnimationRenderMode>>
      animation_mode_reset_;

  void MouseEventTriggersInkDropHelper(InkDropMode ink_drop_mode);

 private:
  DISALLOW_COPY_AND_ASSIGN(InkDropHostViewTest);
};

InkDropHostViewTest::InkDropHostViewTest()
    : test_api_(&host_view_),
      animation_mode_reset_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
          gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED)) {}

InkDropHostViewTest::~InkDropHostViewTest() = default;

void InkDropHostViewTest::MouseEventTriggersInkDropHelper(
    InkDropMode ink_drop_mode) {
  test_api_.SetInkDropMode(ink_drop_mode);
  host_view_.SetEnabled(true);

  // Call GetInkDrop() to make sure the test CreateInkDrop() is created.
  test_api_.GetInkDrop();
  if (ink_drop_mode != InkDropMode::OFF)
    EXPECT_FALSE(host_view_.last_created_inkdrop()->is_hovered());
  else
    EXPECT_EQ(host_view_.last_created_inkdrop(), nullptr);

  ui::MouseEvent mouse_event(ui::ET_MOUSE_ENTERED, gfx::Point(0, 0),
                             gfx::Point(0, 0), ui::EventTimeForNow(),
                             ui::EF_IS_SYNTHESIZED, 0);

  host_view_.GetTargetHandler()->OnEvent(&mouse_event);

  if (ink_drop_mode != InkDropMode::OFF)
    EXPECT_TRUE(host_view_.last_created_inkdrop()->is_hovered());
  else
    EXPECT_EQ(host_view_.last_created_inkdrop(), nullptr);
}

// Verifies the return value of GetInkDropCenterBasedOnLastEvent() for a null
// Event.
TEST_F(InkDropHostViewTest, GetInkDropCenterBasedOnLastEventForNullEvent) {
  host_view_.SetSize(gfx::Size(20, 20));
  test_api_.AnimateInkDrop(InkDropState::ACTION_PENDING, nullptr);
  EXPECT_EQ(gfx::Point(10, 10), test_api_.GetInkDropCenterBasedOnLastEvent());
}

// Verifies the return value of GetInkDropCenterBasedOnLastEvent() for a located
// Event.
TEST_F(InkDropHostViewTest, GetInkDropCenterBasedOnLastEventForLocatedEvent) {
  host_view_.SetSize(gfx::Size(20, 20));

  ui::MouseEvent located_event(ui::ET_MOUSE_PRESSED, gfx::Point(5, 6),
                               gfx::Point(5, 6), ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);

  test_api_.AnimateInkDrop(InkDropState::ACTION_PENDING, &located_event);
  EXPECT_EQ(gfx::Point(5, 6), test_api_.GetInkDropCenterBasedOnLastEvent());
}

TEST_F(InkDropHostViewTest, HasInkDrop) {
  EXPECT_FALSE(test_api_.HasInkDrop());

  test_api_.GetInkDrop();
  EXPECT_TRUE(test_api_.HasInkDrop());

  test_api_.SetInkDropMode(InkDropMode::OFF);
  EXPECT_FALSE(test_api_.HasInkDrop());
}

TEST_F(InkDropHostViewTest, OnInkDropCreatedOnlyNotfiedOnCreation) {
  EXPECT_EQ(0, host_view_.on_ink_drop_created_count());

  test_api_.GetInkDrop();
  EXPECT_EQ(1, host_view_.on_ink_drop_created_count());

  test_api_.GetInkDrop();
  EXPECT_EQ(1, host_view_.on_ink_drop_created_count());

  test_api_.SetInkDropMode(InkDropMode::OFF);
  test_api_.SetInkDropMode(InkDropMode::ON);
  EXPECT_EQ(1, host_view_.on_ink_drop_created_count());

  test_api_.GetInkDrop();
  EXPECT_EQ(2, host_view_.on_ink_drop_created_count());
}

// Verifies that mouse events trigger ink drops when ink drop mode is ON.
TEST_F(InkDropHostViewTest, MouseEventsTriggerInkDropsWhenInkDropIsOn) {
  MouseEventTriggersInkDropHelper(InkDropMode::ON);
}

// Verifies that mouse events trigger ink drops when ink drop mode is
// ON_NO_GESTURE_HANDLER.
TEST_F(InkDropHostViewTest,
       MouseEventsTriggerInkDropsWhenInkDropIsOnNoGestureHandler) {
  MouseEventTriggersInkDropHelper(InkDropMode::ON_NO_GESTURE_HANDLER);
}

// Verifies that mouse events do not trigger ink drops when ink drop mode is
// OFF.
TEST_F(InkDropHostViewTest, MouseEventsDontTriggerInkDropsWhenInkDropIsOff) {
  MouseEventTriggersInkDropHelper(InkDropMode::OFF);
}

// Verifies that ink drops are not shown when the host is disabled.
TEST_F(InkDropHostViewTest,
       GestureEventsDontTriggerInkDropsWhenHostIsDisabled) {
  test_api_.SetInkDropMode(InkDropMode::ON);
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
       {InkDropMode::ON_NO_GESTURE_HANDLER, InkDropMode::OFF}) {
    test_api_.SetInkDropMode(ink_drop_mode);
    ui::GestureEvent gesture_event(
        0.f, 0.f, 0, ui::EventTimeForNow(),
        ui::GestureEventDetails(ui::ET_GESTURE_TAP_DOWN));

    host_view_.GetTargetHandler()->OnEvent(&gesture_event);

    EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
              InkDropState::HIDDEN);
  }
}

#if defined(OS_WIN)
TEST_F(InkDropHostViewTest, NoInkDropOnTouchOrGestureEvents) {
  host_view_.SetSize(gfx::Size(20, 20));

  test_api_.SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);

  // Ensure the target ink drop is in the expected state.
  EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
            InkDropState::HIDDEN);

  ui::TouchEvent touch_event(
      ui::ET_TOUCH_PRESSED, gfx::Point(5, 6), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 1));

  test_api_.AnimateInkDrop(InkDropState::ACTION_PENDING, &touch_event);
  EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
            InkDropState::HIDDEN);

  test_api_.AnimateInkDrop(InkDropState::ALTERNATE_ACTION_PENDING,
                           &touch_event);
  EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
            InkDropState::HIDDEN);

  ui::GestureEvent gesture_event(5.0f, 6.0f, 0, ui::EventTimeForNow(),
                                 ui::GestureEventDetails(ui::ET_GESTURE_TAP));

  test_api_.AnimateInkDrop(InkDropState::ACTION_PENDING, &gesture_event);
  EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
            InkDropState::HIDDEN);

  test_api_.AnimateInkDrop(InkDropState::ALTERNATE_ACTION_PENDING,
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

  test_api_.SetInkDropMode(InkDropMode::ON_NO_GESTURE_HANDLER);

  // Ensure the target ink drop is in the expected state.
  EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
            InkDropState::HIDDEN);

  ui::MouseEvent mouse_event(ui::ET_MOUSE_PRESSED, gfx::Point(5, 6),
                             gfx::Point(5, 6), ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);

  test_api_.AnimateInkDrop(InkDropState::ACTION_PENDING, &mouse_event);
  EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
            InkDropState::ACTION_PENDING);

  ui::TouchEvent touch_event(
      ui::ET_TOUCH_PRESSED, gfx::Point(5, 6), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 1));

  test_api_.AnimateInkDrop(InkDropState::ACTION_TRIGGERED, &touch_event);
  EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
            InkDropState::ACTION_TRIGGERED);
}
#endif

}  // namespace test
}  // namespace views
