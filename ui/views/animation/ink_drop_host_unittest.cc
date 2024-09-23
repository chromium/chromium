// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/animation/ink_drop_host.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
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
#include "ui/views/animation/test/ink_drop_host_test_api.h"
#include "ui/views/animation/test/ink_drop_impl_test_api.h"
#include "ui/views/animation/test/test_ink_drop.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/test/views_test_base.h"

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
  }

  TestViewWithInkDrop(const TestViewWithInkDrop&) = delete;
  TestViewWithInkDrop& operator=(const TestViewWithInkDrop&) = delete;

  // Expose EventTarget::target_handler() for testing.
  ui::EventHandler* GetTargetHandler() { return target_handler(); }

  TestInkDrop* last_created_inkdrop() const { return last_created_inkdrop_; }
  void ClearLastCreatedInkDrop() { last_created_inkdrop_ = nullptr; }

 private:
  raw_ptr<TestInkDrop> last_created_inkdrop_ = nullptr;
};

class InkDropHostTest : public testing::Test {
 public:
  InkDropHostTest();
  InkDropHostTest(const InkDropHostTest&) = delete;
  InkDropHostTest& operator=(const InkDropHostTest&) = delete;
  ~InkDropHostTest() override;

 protected:
  // Test target.
  TestViewWithInkDrop host_view_;

  // Provides internal access to |host_view_| test target.
  InkDropHostTestApi test_api_;

  gfx::AnimationTestApi::RenderModeResetter animation_mode_reset_;

  void MouseEventTriggersInkDropHelper(InkDropMode ink_drop_mode);
};

InkDropHostTest::InkDropHostTest()
    : test_api_(InkDrop::Get(&host_view_)),
      animation_mode_reset_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
          gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED)) {}

InkDropHostTest::~InkDropHostTest() = default;

void InkDropHostTest::MouseEventTriggersInkDropHelper(
    InkDropMode ink_drop_mode) {
  test_api_.SetInkDropMode(ink_drop_mode);
  host_view_.SetEnabled(true);

  // Call InkDrop::Get(this)->GetInkDrop() to make sure the test
  // CreateInkDrop() is created.
  test_api_.GetInkDrop();
  if (ink_drop_mode != views::InkDropHost::InkDropMode::OFF) {
    EXPECT_FALSE(host_view_.last_created_inkdrop()->is_hovered());
  } else {
    EXPECT_EQ(host_view_.last_created_inkdrop(), nullptr);
  }

  ui::MouseEvent mouse_event(ui::EventType::kMouseEntered, gfx::Point(0, 0),
                             gfx::Point(0, 0), ui::EventTimeForNow(),
                             ui::EF_IS_SYNTHESIZED, 0);

  host_view_.GetTargetHandler()->OnEvent(&mouse_event);

  if (ink_drop_mode != views::InkDropHost::InkDropMode::OFF) {
    EXPECT_TRUE(host_view_.last_created_inkdrop()->is_hovered());
  } else {
    EXPECT_EQ(host_view_.last_created_inkdrop(), nullptr);
  }
}

// Verifies the return value of GetInkDropCenterBasedOnLastEvent() for a null
// Event.
TEST_F(InkDropHostTest, GetInkDropCenterBasedOnLastEventForNullEvent) {
  host_view_.SetSize(gfx::Size(20, 20));
  test_api_.AnimateToState(InkDropState::ACTION_PENDING, nullptr);
  EXPECT_EQ(gfx::Point(10, 10),
            InkDrop::Get(&host_view_)->GetInkDropCenterBasedOnLastEvent());
}

// Verifies the return value of GetInkDropCenterBasedOnLastEvent() for a located
// Event.
TEST_F(InkDropHostTest, GetInkDropCenterBasedOnLastEventForLocatedEvent) {
  host_view_.SetSize(gfx::Size(20, 20));

  ui::MouseEvent located_event(ui::EventType::kMousePressed, gfx::Point(5, 6),
                               gfx::Point(5, 6), ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);

  test_api_.AnimateToState(InkDropState::ACTION_PENDING, &located_event);
  EXPECT_EQ(gfx::Point(5, 6),
            InkDrop::Get(&host_view_)->GetInkDropCenterBasedOnLastEvent());
}

TEST_F(InkDropHostTest, HasInkDrop) {
  EXPECT_FALSE(test_api_.HasInkDrop());

  test_api_.GetInkDrop();
  EXPECT_TRUE(test_api_.HasInkDrop());

  test_api_.SetInkDropMode(views::InkDropHost::InkDropMode::OFF);
  EXPECT_FALSE(test_api_.HasInkDrop());
}

// Verifies that mouse events trigger ink drops when ink drop mode is ON.
TEST_F(InkDropHostTest, MouseEventsTriggerInkDropsWhenInkDropIsOn) {
  MouseEventTriggersInkDropHelper(views::InkDropHost::InkDropMode::ON);
}

// Verifies that mouse events trigger ink drops when ink drop mode is
// ON_NO_GESTURE_HANDLER.
TEST_F(InkDropHostTest,
       MouseEventsTriggerInkDropsWhenInkDropIsOnNoGestureHandler) {
  MouseEventTriggersInkDropHelper(
      views::InkDropHost::InkDropMode::ON_NO_GESTURE_HANDLER);
}

// Verifies that mouse events do not trigger ink drops when ink drop mode is
// OFF.
TEST_F(InkDropHostTest, MouseEventsDontTriggerInkDropsWhenInkDropIsOff) {
  MouseEventTriggersInkDropHelper(views::InkDropHost::InkDropMode::OFF);
}

// Verifies that ink drops are not shown when the host is disabled.
TEST_F(InkDropHostTest, GestureEventsDontTriggerInkDropsWhenHostIsDisabled) {
  test_api_.SetInkDropMode(views::InkDropHost::InkDropMode::ON);
  host_view_.SetEnabled(false);

  ui::GestureEvent gesture_event(
      0.f, 0.f, 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::EventType::kGestureTapDown));

  host_view_.GetTargetHandler()->OnEvent(&gesture_event);

  EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
            InkDropState::HIDDEN);
}

// Verifies that ink drops are not triggered by gesture events when ink drop
// mode is ON_NO_GESTURE_EVENT or OFF.
TEST_F(InkDropHostTest,
       GestureEventsDontTriggerInkDropsWhenInkDropModeIsNotOn) {
  for (auto ink_drop_mode :
       {views::InkDropHost::InkDropMode::ON_NO_GESTURE_HANDLER,
        views::InkDropHost::InkDropMode::OFF}) {
    test_api_.SetInkDropMode(ink_drop_mode);
    ui::GestureEvent gesture_event(
        0.f, 0.f, 0, ui::EventTimeForNow(),
        ui::GestureEventDetails(ui::EventType::kGestureTapDown));

    host_view_.GetTargetHandler()->OnEvent(&gesture_event);

    EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
              InkDropState::HIDDEN);
    // Subsequent times through the loop will call SetInkDropMode which may
    // delete the TestInkDrop pointed to by TestViewWithInkDrop and cause a
    // dangling pointer.
    host_view_.ClearLastCreatedInkDrop();
  }
}

#if BUILDFLAG(IS_WIN)
TEST_F(InkDropHostTest, NoInkDropOnTouchOrGestureEvents) {
  host_view_.SetSize(gfx::Size(20, 20));

  test_api_.SetInkDropMode(
      views::InkDropHost::InkDropMode::ON_NO_GESTURE_HANDLER);

  // Ensure the target ink drop is in the expected state.
  EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
            InkDropState::HIDDEN);

  ui::TouchEvent touch_event(
      ui::EventType::kTouchPressed, gfx::Point(5, 6), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));

  test_api_.AnimateToState(InkDropState::ACTION_PENDING, &touch_event);
  EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
            InkDropState::HIDDEN);

  test_api_.AnimateToState(InkDropState::ALTERNATE_ACTION_PENDING,
                           &touch_event);
  EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
            InkDropState::HIDDEN);

  ui::GestureEvent gesture_event(
      5.0f, 6.0f, 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::EventType::kGestureTap));

  test_api_.AnimateToState(InkDropState::ACTION_PENDING, &gesture_event);
  EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
            InkDropState::HIDDEN);

  test_api_.AnimateToState(InkDropState::ALTERNATE_ACTION_PENDING,
                           &gesture_event);
  EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
            InkDropState::HIDDEN);
}

TEST_F(InkDropHostTest, DismissInkDropOnTouchOrGestureEvents) {
  // TODO(bruthig): Re-enable! For some reason these tests fail on some win
  // trunk builds. See crbug.com/731811.
  if (!gfx::Animation::ShouldRenderRichAnimation()) {
    return;
  }

  host_view_.SetSize(gfx::Size(20, 20));

  test_api_.SetInkDropMode(
      views::InkDropHost::InkDropMode::ON_NO_GESTURE_HANDLER);

  // Ensure the target ink drop is in the expected state.
  EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
            InkDropState::HIDDEN);

  ui::MouseEvent mouse_event(ui::EventType::kMousePressed, gfx::Point(5, 6),
                             gfx::Point(5, 6), ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);

  test_api_.AnimateToState(InkDropState::ACTION_PENDING, &mouse_event);
  EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
            InkDropState::ACTION_PENDING);

  ui::TouchEvent touch_event(
      ui::EventType::kTouchPressed, gfx::Point(5, 6), ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::kTouch, 1));

  test_api_.AnimateToState(InkDropState::ACTION_TRIGGERED, &touch_event);
  EXPECT_EQ(test_api_.GetInkDrop()->GetTargetInkDropState(),
            InkDropState::ACTION_TRIGGERED);
}
#endif

// Verifies that calling OnInkDropHighlightedChanged() triggers a property
// changed notification for the highlighted property.
TEST_F(InkDropHostTest, HighlightedChangedFired) {
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
  }
  BasicTestViewWithInkDrop(const BasicTestViewWithInkDrop&) = delete;
  BasicTestViewWithInkDrop& operator=(const BasicTestViewWithInkDrop&) = delete;
  ~BasicTestViewWithInkDrop() override = default;
};

// Tests the existence of layer clipping or layer masking when certain path
// generators are applied on an InkDropHost.
class InkDropHostClippingTest : public testing::Test {
 public:
  InkDropHostClippingTest() : host_view_test_api_(InkDrop::Get(&host_view_)) {
    // Set up an InkDropHost. Clipping is based on the size of the view, so
    // make sure the size is non empty.
    host_view_test_api_.SetInkDropMode(views::InkDropHost::InkDropMode::ON);
    host_view_.SetSize(gfx::Size(20, 20));

    // The root layer of the ink drop is created the first time GetInkDrop is
    // called and then kept alive until the host view is destroyed.
    ink_drop_ =
        static_cast<InkDropImpl*>(InkDrop::Get(&host_view_)->GetInkDrop());
    ink_drop_test_api_ = std::make_unique<test::InkDropImplTestApi>(ink_drop_);
  }
  InkDropHostClippingTest(const InkDropHostClippingTest&) = delete;
  InkDropHostClippingTest& operator=(const InkDropHostClippingTest&) = delete;
  ~InkDropHostClippingTest() override = default;

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
TEST_F(InkDropHostClippingTest, DefaultInkDropMasksRootLayer) {
  ink_drop_->SetHovered(true);
  EXPECT_TRUE(GetRootLayer()->layer_mask_layer());
  EXPECT_TRUE(GetRootLayer()->clip_rect().IsEmpty());
}

// Tests that when adding a non empty highlight path generator, the root layer
// is clipped instead of masked.
TEST_F(InkDropHostClippingTest,
       HighlightPathGeneratorClipsRootLayerWithoutMask) {
  views::InstallRectHighlightPathGenerator(&host_view_);
  ink_drop_->SetHovered(true);
  EXPECT_FALSE(GetRootLayer()->layer_mask_layer());
  EXPECT_FALSE(GetRootLayer()->clip_rect().IsEmpty());
}

// An empty highlight path generator is used for views who do not want their
// highlight or ripple constrained by their size. Test that the views' ink
// drop root layers have neither a clip or mask.
TEST_F(InkDropHostClippingTest,
       EmptyHighlightPathGeneratorUsesNeitherMaskNorClipsRootLayer) {
  views::InstallEmptyHighlightPathGenerator(&host_view_);
  ink_drop_->SetHovered(true);
  EXPECT_FALSE(GetRootLayer()->layer_mask_layer());
  EXPECT_TRUE(GetRootLayer()->clip_rect().IsEmpty());
}

class InkDropInWidgetTest : public ViewsTestBase {
 public:
  InkDropInWidgetTest() = default;
  InkDropInWidgetTest(const InkDropInWidgetTest&) = delete;
  InkDropInWidgetTest& operator=(const InkDropInWidgetTest&) = delete;
  ~InkDropInWidgetTest() override = default;

 protected:
  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
    view_ =
        widget_->SetContentsView(std::make_unique<BasicTestViewWithInkDrop>());
  }

  void TearDown() override {
    view_ = nullptr;
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  InkDropHost& ink_drop() { return *InkDrop::Get(view_); }
  const ui::ColorProvider& color_provider() {
    return *widget_->GetColorProvider();
  }

 private:
  std::unique_ptr<Widget> widget_;
  raw_ptr<View> view_ = nullptr;
};

TEST_F(InkDropInWidgetTest, SetBaseColor) {
  ink_drop().SetBaseColor(SK_ColorBLUE);
  EXPECT_EQ(ink_drop().GetBaseColor(), SK_ColorBLUE);
}

TEST_F(InkDropInWidgetTest, SetBaseColorId) {
  ink_drop().SetBaseColorId(ui::kColorSeparator);
  EXPECT_EQ(ink_drop().GetBaseColor(),
            color_provider().GetColor(ui::kColorSeparator));

  ink_drop().SetBaseColor(SK_ColorBLUE);
  EXPECT_EQ(ink_drop().GetBaseColor(), SK_ColorBLUE);
}

TEST_F(InkDropInWidgetTest, SetBaseColorCallback) {
  base::MockRepeatingCallback<SkColor()> callback;
  EXPECT_CALL(callback, Run).WillRepeatedly(testing::Return(SK_ColorCYAN));
  ink_drop().SetBaseColorCallback(callback.Get());
  EXPECT_EQ(ink_drop().GetBaseColor(), SK_ColorCYAN);

  ink_drop().SetBaseColor(SK_ColorBLUE);
  EXPECT_EQ(ink_drop().GetBaseColor(), SK_ColorBLUE);
}

// This fixture tests attention state.
class InkDropHostAttentionTest : public ViewsTestBase {
 public:
  InkDropHostAttentionTest() = default;
  InkDropHostAttentionTest(const InkDropHostAttentionTest&) = delete;
  InkDropHostAttentionTest& operator=(const InkDropHostAttentionTest&) = delete;
  ~InkDropHostAttentionTest() override = default;

  ui::Layer* GetRootLayer() { return ink_drop_test_api_->GetRootLayer(); }

 protected:
  void SetUp() override {
    ViewsTestBase::SetUp();
    host_view_ = std::make_unique<views::View>();
    InkDrop::Install(host_view_.get(),
                     std::make_unique<InkDropHost>(host_view_.get()));
    ink_drop_host_ = InkDrop::Get(host_view_.get());
    widget_ = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
    ink_drop_host_test_api_ =
        std::make_unique<InkDropHostTestApi>(ink_drop_host_);

    // Set up an InkDropHost. Clipping is based on the size of the view, so
    // make sure the size is non empty.
    ink_drop_host_->SetMode(views::InkDropHost::InkDropMode::ON);
    host_view_->SetSize(gfx::Size(20, 20));

    // The root layer of the ink drop is created the first time GetInkDrop is
    // called and then kept alive until the host view is destroyed.
    ink_drop_ =
        static_cast<InkDropImpl*>(InkDrop::Get(host_view_.get())->GetInkDrop());
    ink_drop_test_api_ = std::make_unique<test::InkDropImplTestApi>(ink_drop_);

    widget_->SetContentsView(host_view_.get());
  }

  void TearDown() override {
    ink_drop_ = nullptr;
    ink_drop_host_ = nullptr;
    ink_drop_test_api_.reset();
    ink_drop_host_test_api_.reset();
    host_view_.reset();
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  InkDropHost* ink_drop_host() { return ink_drop_host_; }

  const ui::ColorProvider& color_provider() {
    return *widget_->GetColorProvider();
  }

  std::unique_ptr<Widget> widget_;

  // Test target.
  std::unique_ptr<views::View> host_view_;

  // Provides internal access to ink drop host.
  std::unique_ptr<InkDropHostTestApi> ink_drop_host_test_api_;

  // Provides internal access to `host_view_`'s ink drop.
  std::unique_ptr<test::InkDropImplTestApi> ink_drop_test_api_;

  raw_ptr<InkDropHost> ink_drop_host_ = nullptr;

  raw_ptr<InkDropImpl> ink_drop_ = nullptr;
};

TEST_F(InkDropHostAttentionTest, ToggleAttentionColor) {
  // Give it an original color before flipping attention to true.
  ink_drop_host()->SetBaseColorId(ui::kColorSeparator);
  EXPECT_EQ(ink_drop_host()->GetBaseColor(),
            color_provider().GetColor(ui::kColorSeparator));

  // Flipping attention state to true triggers attention color.
  ink_drop_host()->ToggleAttentionState(true);
  EXPECT_EQ(
      ink_drop_host()->GetBaseColor(),
      color_provider().GetColor(ui::kColorButtonFeatureAttentionHighlight));

  // Flipping attention state to false triggers color restore to original.
  ink_drop_host()->ToggleAttentionState(false);
  EXPECT_EQ(ink_drop_host()->GetBaseColor(),
            color_provider().GetColor(ui::kColorSeparator));
}

TEST_F(InkDropHostAttentionTest, ToggleAttentionMask) {
  // Set default state.
  ink_drop_->SetHovered(true);
  EXPECT_TRUE(GetRootLayer()->layer_mask_layer());

  // Manually remove ink drop mask.
  ink_drop_host_test_api_->RemoveInkDropMask();
  EXPECT_FALSE(GetRootLayer()->layer_mask_layer());

  // Flipping attention to true triggers pulsing mask applied.
  ink_drop_host()->ToggleAttentionState(true);
  EXPECT_TRUE(GetRootLayer()->layer_mask_layer());
}

TEST_F(InkDropHostAttentionTest, AttentionMaskCoexistWithClipping) {
  views::InstallRectHighlightPathGenerator(host_view_.get());

  // Clipping suppresses mask with attention off.
  ink_drop_->SetHovered(true);
  EXPECT_FALSE(GetRootLayer()->layer_mask_layer());

  // Attention mask can be added while clipping is on.
  ink_drop_host()->ToggleAttentionState(true);
  EXPECT_TRUE(GetRootLayer()->layer_mask_layer());
  EXPECT_FALSE(GetRootLayer()->clip_rect().IsEmpty());
}

}  // namespace views::test
