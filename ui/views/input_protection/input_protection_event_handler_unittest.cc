// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/input_protection_event_handler.h"

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/ax_platform_for_test.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/input_protection/occluded_widget_input_protector.h"
#include "ui/views/metrics.h"
#include "ui/views/test/mock_input_event_activation_protector.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/views_features.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

namespace views::test {
namespace {

using ::testing::_;
using ::testing::Return;

// Observes whether dispatched events receive the `kPropertyInputProtected`
// property during input protection processing.
class PropertyCheckingHandler : public ui::EventHandler {
 public:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::EventType::kMousePressed) {
      saw_property_ = event->properties() &&
                      event->properties()->contains(kPropertyInputProtected);
    }
  }
  bool saw_property() const { return saw_property_; }

 private:
  bool saw_property_ = false;
};

// Represents a gesture tap event marked as non-cancelable to verify that
// input protection skips evaluating events that cannot be stopped.
class NonCancelableGestureEvent : public ui::GestureEvent {
 public:
  explicit NonCancelableGestureEvent(const gfx::Point& location)
      : ui::GestureEvent(location.x(),
                         location.y(),
                         0,
                         ui::EventTimeForNow(),
                         ui::GestureEventDetails(ui::EventType::kGestureTap)) {
    set_cancelable(false);
  }
};

// Test view that overrides `SkipDefaultKeyEventProcessing` to return true,
// rather than allowing default `FocusManager` traversal.
class SkipDefaultKeyEventProcessingTestView : public View {
  METADATA_HEADER(SkipDefaultKeyEventProcessingTestView, View)

 public:
  SkipDefaultKeyEventProcessingTestView() {
    SetFocusBehavior(FocusBehavior::ALWAYS);
  }
  ~SkipDefaultKeyEventProcessingTestView() override = default;

  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override {
    return true;
  }
};

BEGIN_METADATA(SkipDefaultKeyEventProcessingTestView)
END_METADATA

}  // namespace

class InputProtectionEventHandlerTest : public ViewsTestBase {
 public:
  InputProtectionEventHandlerTest()
      : ViewsTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~InputProtectionEventHandlerTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    widget_ = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget_->SetBounds(gfx::Rect(0, 0, 400, 400));

    auto* contents = widget_->SetContentsView(std::make_unique<View>());
    auto button = std::make_unique<LabelButton>(
        base::BindRepeating(&InputProtectionEventHandlerTest::OnButtonClicked,
                            base::Unretained(this)),
        u"Button");
    button->SetBounds(10, 10, 100, 40);
    button_ = contents->AddChildView(std::move(button));
    widget_->Show();

    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        GetContext(), widget_->GetNativeWindow());
  }

  void TearDown() override {
    OccludedWidgetInputProtector::GetInstance()->ClearForTesting();
    event_generator_.reset();
    button_ = nullptr;
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  void OnButtonClicked() { button_click_count_++; }

  void EnableInputProtection(bool should_block) {
    auto mock_protector = std::make_unique<MockInputEventActivationProtector>();
    ON_CALL(*mock_protector, IsPossiblyUnintendedInteraction(_, _, _))
        .WillByDefault(Return(should_block));
    EXPECT_CALL(*mock_protector, IsPossiblyUnintendedInteraction(_, _, _))
        .Times(::testing::AtLeast(1));
    widget_->EnableInputEventActivationProtection(std::move(mock_protector));
  }

  ui::MouseEvent CreateMouseEvent(ui::EventType type,
                                  const gfx::Point& screen_point) const {
    gfx::Point local_point = screen_point;
    View::ConvertPointFromScreen(widget_->GetRootView(), &local_point);
    return ui::MouseEvent(type, local_point, screen_point,
                          ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                          ui::EF_LEFT_MOUSE_BUTTON);
  }

  ui::GestureEvent CreateGestureEvent(ui::EventType type,
                                      const gfx::Point& screen_point) const {
    gfx::Point local_point = screen_point;
    View::ConvertPointFromScreen(widget_->GetRootView(), &local_point);
    return ui::GestureEvent(local_point.x(), local_point.y(), 0,
                            ui::EventTimeForNow(),
                            ui::GestureEventDetails(type));
  }

  Widget* widget() const { return widget_.get(); }
  LabelButton* button() const { return button_; }
  ui::test::EventGenerator* event_generator() const {
    return event_generator_.get();
  }
  int button_click_count() const { return button_click_count_; }

 private:
  std::unique_ptr<Widget> widget_;
  raw_ptr<LabelButton> button_ = nullptr;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  int button_click_count_ = 0;

  base::test::ScopedFeatureList scoped_feature_list_{
      features::kEnableInputProtection};
};

TEST_F(InputProtectionEventHandlerTest, MousePressBlockedAndReleaseDiscarded) {
  EnableInputProtection(/*should_block=*/true);

  const gfx::Point button_center = button()->GetBoundsInScreen().CenterPoint();
  event_generator()->MoveMouseTo(button_center);

  // Verify that the mouse press is intercepted and blocked by input protection,
  // preventing the button from entering the pressed state.
  event_generator()->PressLeftButton();
  EXPECT_NE(button()->GetState(), Button::ButtonState::STATE_PRESSED);
  EXPECT_EQ(button_click_count(), 0);

  // Verify that the mouse release is ignored by `RootView` because the handler
  // state was reset above on the blocked press.
  event_generator()->ReleaseLeftButton();
  EXPECT_EQ(button_click_count(), 0);
}

TEST_F(InputProtectionEventHandlerTest, MousePressAllowedTriggersClick) {
  EnableInputProtection(/*should_block=*/false);

  const gfx::Point button_center = button()->GetBoundsInScreen().CenterPoint();
  event_generator()->MoveMouseTo(button_center);

  // Verify that the allowed mouse press transitions the button to pressed
  // state.
  event_generator()->PressLeftButton();
  EXPECT_EQ(button()->GetState(), Button::ButtonState::STATE_PRESSED);

  // Verify that releasing the mouse completes the click.
  event_generator()->ReleaseLeftButton();
  EXPECT_EQ(button_click_count(), 1);
}

// No touch on desktop Mac. Tracked on http://crbug.com/445520.
#if !BUILDFLAG(IS_MAC)
TEST_F(InputProtectionEventHandlerTest, TouchPressBlocked) {
  EnableInputProtection(/*should_block=*/true);

  const gfx::Point button_center = button()->GetBoundsInScreen().CenterPoint();

  event_generator()->PressTouch(button_center);
  EXPECT_EQ(button_click_count(), 0);

  event_generator()->ReleaseTouch();
  EXPECT_EQ(button_click_count(), 0);
}
#endif  // !BUILDFLAG(IS_MAC)

TEST_F(InputProtectionEventHandlerTest, GestureTapsBlocked) {
  EnableInputProtection(/*should_block=*/true);

  const gfx::Point button_center = button()->GetBoundsInScreen().CenterPoint();
  ui::GestureEvent tap =
      CreateGestureEvent(ui::EventType::kGestureTap, button_center);

  widget()->OnGestureEvent(&tap);
  EXPECT_TRUE(tap.stopped_propagation());
  EXPECT_EQ(button_click_count(), 0);
}

TEST_F(InputProtectionEventHandlerTest, UnprotectedWidgetAllowsEvents) {
  // Verify that input protection is not enabled on the widget.
  EXPECT_FALSE(widget()->IsInputEventActivationProtectionEnabled());

  const gfx::Point button_center = button()->GetBoundsInScreen().CenterPoint();

  event_generator()->MoveMouseTo(button_center);
  event_generator()->PressLeftButton();
  event_generator()->ReleaseLeftButton();
  EXPECT_EQ(button_click_count(), 1);
}

TEST_F(InputProtectionEventHandlerTest,
       UnprotectedWidgetBlocksClicksWhenOccludedByAlwaysOnTopWindow) {
  // Verify that widget-level input activation protection is not enabled.
  EXPECT_FALSE(widget()->IsInputEventActivationProtectionEnabled());

  // Create an always-on-top widget that occludes the button.
  gfx::Rect aot_bounds = button()->GetBoundsInScreen();
  aot_bounds.Outset(20);

  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
  params.remove_standard_frame = true;
  params.ownership = Widget::InitParams::CLIENT_OWNS_WIDGET;
  params.bounds = aot_bounds;
  auto aot_widget = std::make_unique<Widget>();
  aot_widget->Init(std::move(params));
  aot_widget->Show();

  const gfx::Point button_center = button()->GetBoundsInScreen().CenterPoint();
  ASSERT_TRUE(aot_widget->GetNonDecoratedClientAreaBoundsInScreen().Contains(
      button_center));

  // Click on the button in the unprotected widget while occluded by the AOT
  // window. The click should be blocked by global occlusion protection.
  event_generator()->MoveMouseTo(button_center);
  event_generator()->PressLeftButton();
  event_generator()->ReleaseLeftButton();
  EXPECT_EQ(button_click_count(), 0);

  // Hide the AOT widget to simulate a pop-away attack.
  aot_widget->Hide();

  // Recent occlusion: Clicks during the cooldown interval after hiding the AOT
  // window are blocked.
  event_generator()->MoveMouseTo(button_center);
  event_generator()->PressLeftButton();
  event_generator()->ReleaseLeftButton();
  EXPECT_EQ(button_click_count(), 0);

  // Fast-forward past the occlusion cooldown interval.
  task_environment()->FastForwardBy(GetDoubleClickInterval() +
                                    base::Milliseconds(1));

  // Now that the occlusion has expired, the click should succeed.
  event_generator()->MoveMouseTo(button_center);
  event_generator()->PressLeftButton();
  event_generator()->ReleaseLeftButton();
  EXPECT_EQ(button_click_count(), 1);
}

TEST_F(InputProtectionEventHandlerTest,
       ProtectedWidgetBlocksClicksWhenOccludedByAlwaysOnTopWindow) {
  // Configure the widget policy to allow input so that blocking is only driven
  // by global occlusion protection.
  EnableInputProtection(/*should_block=*/false);
  EXPECT_TRUE(widget()->IsInputEventActivationProtectionEnabled());

  // Create an always-on-top widget that occludes the button.
  gfx::Rect aot_bounds = button()->GetBoundsInScreen();
  aot_bounds.Outset(20);

  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
  params.remove_standard_frame = true;
  params.ownership = Widget::InitParams::CLIENT_OWNS_WIDGET;
  params.bounds = aot_bounds;
  auto aot_widget = std::make_unique<Widget>();
  aot_widget->Init(std::move(params));
  aot_widget->Show();

  const gfx::Point button_center = button()->GetBoundsInScreen().CenterPoint();
  ASSERT_TRUE(aot_widget->GetNonDecoratedClientAreaBoundsInScreen().Contains(
      button_center));

  // Current occlusion: Click while occluded is blocked by global occlusion.
  event_generator()->MoveMouseTo(button_center);
  event_generator()->PressLeftButton();
  event_generator()->ReleaseLeftButton();
  EXPECT_EQ(button_click_count(), 0);

  // Hide the AOT widget to simulate a pop-away attack.
  aot_widget->Hide();

  // Recent occlusion: Clicks during the cooldown interval after hiding the AOT
  // window are blocked.
  event_generator()->MoveMouseTo(button_center);
  event_generator()->PressLeftButton();
  event_generator()->ReleaseLeftButton();
  EXPECT_EQ(button_click_count(), 0);

  // Fast-forward past the occlusion cooldown interval.
  task_environment()->FastForwardBy(GetDoubleClickInterval() +
                                    base::Milliseconds(1));

  // Now that the occlusion has expired, the click should succeed.
  event_generator()->MoveMouseTo(button_center);
  event_generator()->PressLeftButton();
  event_generator()->ReleaseLeftButton();
  EXPECT_EQ(button_click_count(), 1);
}

TEST_F(InputProtectionEventHandlerTest,
       MultipleWidgetsHaveIndependentProtection) {
  // Widget 1 has input protection enabled and set to block.
  EnableInputProtection(/*should_block=*/true);
  EXPECT_TRUE(widget()->IsInputEventActivationProtectionEnabled());

  // Create a second widget that is unprotected.
  auto widget2 = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET);
  widget2->SetBounds(gfx::Rect(450, 0, 400, 400));
  auto* contents2 = widget2->SetContentsView(std::make_unique<View>());
  EXPECT_FALSE(widget2->IsInputEventActivationProtectionEnabled());

  int widget2_clicks = 0;
  auto button2 = std::make_unique<LabelButton>(
      base::BindRepeating([](int* count) { (*count)++; }, &widget2_clicks),
      u"Button 2");
  button2->SetBounds(10, 10, 100, 40);
  auto* button2_ptr = contents2->AddChildView(std::move(button2));
  widget2->Show();

  // Click on Widget 1: should be blocked.
  event_generator()->MoveMouseTo(button()->GetBoundsInScreen().CenterPoint());
  event_generator()->PressLeftButton();
  event_generator()->ReleaseLeftButton();
  EXPECT_EQ(button_click_count(), 0);

  // Click on Widget 2: should succeed.
  event_generator()->SetTargetWindow(widget2->GetNativeWindow());
  event_generator()->MoveMouseTo(
      button2_ptr->GetBoundsInScreen().CenterPoint());
  event_generator()->PressLeftButton();
  event_generator()->ReleaseLeftButton();
  EXPECT_EQ(widget2_clicks, 1);
}

TEST_F(InputProtectionEventHandlerTest,
       EventWithInputProtectedPropertySkipsEvaluation) {
  auto mock_protector =
      std::make_unique<testing::NiceMock<MockInputEventActivationProtector>>();
  auto* mock_protector_ptr = mock_protector.get();

  // Verify that the protector is not queried (0 calls) because the mouse press
  // is marked with `kPropertyInputProtected`, which tells
  // `InputProtectionEventHandler` that the event was already evaluated and
  // should skip further checks.
  EXPECT_CALL(*mock_protector_ptr, IsPossiblyUnintendedInteraction(_, _, _))
      .Times(0);
  widget()->EnableInputEventActivationProtection(std::move(mock_protector));

  const gfx::Point button_center = button()->GetBoundsInScreen().CenterPoint();

  ui::MouseEvent press =
      CreateMouseEvent(ui::EventType::kMousePressed, button_center);
  press.SetProperty(kPropertyInputProtected, std::vector<uint8_t>());
  widget()->OnMouseEvent(&press);

  ui::MouseEvent release =
      CreateMouseEvent(ui::EventType::kMouseReleased, button_center);
  widget()->OnMouseEvent(&release);

  // The event skipped input protection evaluation and clicked the button.
  EXPECT_EQ(button_click_count(), 1);
}

TEST_F(InputProtectionEventHandlerTest, InputProtectedPropertySetOnEvent) {
  EnableInputProtection(/*should_block=*/false);

  PropertyCheckingHandler observer;
  widget()->GetRootView()->AddPreTargetHandler(&observer);

  const gfx::Point button_center = button()->GetBoundsInScreen().CenterPoint();
  event_generator()->MoveMouseTo(button_center);
  event_generator()->PressLeftButton();

  // Verify that the event was tagged with `kPropertyInputProtected` during
  // input protection processing.
  EXPECT_TRUE(observer.saw_property());
  widget()->GetRootView()->RemovePreTargetHandler(&observer);
}

TEST_F(InputProtectionEventHandlerTest, NonCancelableEventSkipsEvaluation) {
  auto mock_protector =
      std::make_unique<testing::NiceMock<MockInputEventActivationProtector>>();
  auto* mock_protector_ptr = mock_protector.get();

  // Set the protector to block unintended interactions.
  ON_CALL(*mock_protector_ptr, IsPossiblyUnintendedInteraction(_, _, _))
      .WillByDefault(Return(true));
  widget()->EnableInputEventActivationProtection(std::move(mock_protector));

  const gfx::Point button_center = button()->GetBoundsInScreen().CenterPoint();

  // A cancelable gesture tap is evaluated, blocked by input protection, and its
  // propagation is stopped.
  ui::GestureEvent cancelable_tap =
      CreateGestureEvent(ui::EventType::kGestureTap, button_center);
  EXPECT_TRUE(cancelable_tap.cancelable());
  widget()->OnGestureEvent(&cancelable_tap);
  EXPECT_TRUE(cancelable_tap.stopped_propagation());

  // Target point is outside the button because sending the non-cancelable event
  // to the button would crash.
  gfx::Point point_outside_button =
      button()->GetBoundsInScreen().bottom_right() + gfx::Vector2d(10, 10);
  View::ConvertPointFromScreen(widget()->GetRootView(), &point_outside_button);
  NonCancelableGestureEvent non_cancelable_tap(point_outside_button);

  // A non-cancelable gesture tap skips input protection evaluation, and its
  // propagation is not stopped.
  EXPECT_FALSE(non_cancelable_tap.cancelable());
  widget()->OnGestureEvent(&non_cancelable_tap);
  EXPECT_FALSE(non_cancelable_tap.stopped_propagation());
}

TEST_F(InputProtectionEventHandlerTest, KeyPressBlockedForActionKeys) {
  button()->RequestFocus();
  ASSERT_EQ(widget()->GetFocusManager()->GetFocusedView(), button());

  EnableInputProtection(/*should_block=*/true);

  // Pressing Space on the focused button should be evaluated and blocked.
  ui::KeyEvent space_press(ui::EventType::kKeyPressed, ui::VKEY_SPACE,
                           ui::EF_NONE);
  widget()->OnKeyEvent(&space_press);
  EXPECT_TRUE(space_press.stopped_propagation());
  EXPECT_EQ(button_click_count(), 0);
}

TEST_F(InputProtectionEventHandlerTest, KeyPressAllowedTriggersAction) {
  button()->RequestFocus();
  ASSERT_EQ(widget()->GetFocusManager()->GetFocusedView(), button());

  EnableInputProtection(/*should_block=*/false);

  // Pressing and releasing Space on the focused button should trigger a click.
  ui::KeyEvent space_press(ui::EventType::kKeyPressed, ui::VKEY_SPACE,
                           ui::EF_NONE);
  widget()->OnKeyEvent(&space_press);

  ui::KeyEvent space_release(ui::EventType::kKeyReleased, ui::VKEY_SPACE,
                             ui::EF_NONE);
  widget()->OnKeyEvent(&space_release);
  EXPECT_EQ(button_click_count(), 1);
}

TEST_F(InputProtectionEventHandlerTest,
       TabTraversalKeyBypassesInputProtection) {
  // Add a second button to test focus traversal.
  auto button2 =
      std::make_unique<LabelButton>(Button::PressedCallback(), u"Button 2");
  button2->SetBounds(120, 10, 100, 40);
  auto* button2_ptr = button()->parent()->AddChildView(std::move(button2));

  button()->RequestFocus();
  ASSERT_EQ(widget()->GetFocusManager()->GetFocusedView(), button());

  auto mock_protector =
      std::make_unique<testing::NiceMock<MockInputEventActivationProtector>>();
  auto* mock_protector_ptr = mock_protector.get();

  // The protector should not even be called for Tab traversal.
  EXPECT_CALL(*mock_protector_ptr, IsPossiblyUnintendedInteraction(_, _, _))
      .Times(0);
  widget()->EnableInputEventActivationProtection(std::move(mock_protector));

  // Tab key should bypass input protection and advance focus to button 2.
  ui::KeyEvent tab_press(ui::EventType::kKeyPressed, ui::VKEY_TAB, ui::EF_NONE);
  widget()->OnKeyEvent(&tab_press);
  EXPECT_EQ(widget()->GetFocusManager()->GetFocusedView(), button2_ptr);
}

TEST_F(InputProtectionEventHandlerTest, ArrowKeysBypassInputProtection) {
  button()->RequestFocus();

  auto mock_protector =
      std::make_unique<testing::NiceMock<MockInputEventActivationProtector>>();
  auto* mock_protector_ptr = mock_protector.get();

  // Arrow keys are focus traversal keys and should not trigger input protection
  // checks.
  EXPECT_CALL(*mock_protector_ptr, IsPossiblyUnintendedInteraction(_, _, _))
      .Times(0);
  widget()->EnableInputEventActivationProtection(std::move(mock_protector));

  ui::KeyEvent left_arrow(ui::EventType::kKeyPressed, ui::VKEY_LEFT,
                          ui::EF_NONE);
  widget()->OnKeyEvent(&left_arrow);

  ui::KeyEvent right_arrow(ui::EventType::kKeyPressed, ui::VKEY_RIGHT,
                           ui::EF_NONE);
  widget()->OnKeyEvent(&right_arrow);

  ui::KeyEvent up_arrow(ui::EventType::kKeyPressed, ui::VKEY_UP, ui::EF_NONE);
  widget()->OnKeyEvent(&up_arrow);

  ui::KeyEvent down_arrow(ui::EventType::kKeyPressed, ui::VKEY_DOWN,
                          ui::EF_NONE);
  widget()->OnKeyEvent(&down_arrow);
}

TEST_F(InputProtectionEventHandlerTest, KeyReleaseSkipsEvaluation) {
  button()->RequestFocus();

  auto mock_protector =
      std::make_unique<testing::NiceMock<MockInputEventActivationProtector>>();
  auto* mock_protector_ptr = mock_protector.get();

  // Key releases should not trigger input protection checks.
  EXPECT_CALL(*mock_protector_ptr, IsPossiblyUnintendedInteraction(_, _, _))
      .Times(0);
  widget()->EnableInputEventActivationProtection(std::move(mock_protector));

  ui::KeyEvent space_release(ui::EventType::kKeyReleased, ui::VKEY_SPACE,
                             ui::EF_NONE);
  widget()->OnKeyEvent(&space_release);
  EXPECT_FALSE(space_release.stopped_propagation());
}

TEST_F(InputProtectionEventHandlerTest,
       ShiftTabTraversalKeyBypassesInputProtection) {
  // Add a second button to test reverse focus traversal.
  auto button2 =
      std::make_unique<LabelButton>(Button::PressedCallback(), u"Button 2");
  button2->SetBounds(120, 10, 100, 40);
  button()->parent()->AddChildView(std::move(button2));

  // Focus the first button, then advance to the second button.
  button()->RequestFocus();
  widget()->GetFocusManager()->AdvanceFocus(/*reverse=*/false);
  ASSERT_NE(widget()->GetFocusManager()->GetFocusedView(), button());

  auto mock_protector =
      std::make_unique<testing::NiceMock<MockInputEventActivationProtector>>();
  auto* mock_protector_ptr = mock_protector.get();

  // The protector should not be called for Shift-Tab traversal.
  EXPECT_CALL(*mock_protector_ptr, IsPossiblyUnintendedInteraction(_, _, _))
      .Times(0);
  widget()->EnableInputEventActivationProtection(std::move(mock_protector));

  // Shift-Tab key should bypass input protection and advance focus backwards.
  ui::KeyEvent shift_tab_press(ui::EventType::kKeyPressed, ui::VKEY_TAB,
                               ui::EF_SHIFT_DOWN);
  widget()->OnKeyEvent(&shift_tab_press);
  EXPECT_EQ(widget()->GetFocusManager()->GetFocusedView(), button());
}

TEST_F(InputProtectionEventHandlerTest,
       ArrowKeysBlockedForViewsSkippingDefaultProcessing) {
  auto consuming_view =
      std::make_unique<SkipDefaultKeyEventProcessingTestView>();
  consuming_view->SetBounds(120, 10, 100, 40);
  auto* consuming_view_ptr =
      button()->parent()->AddChildView(std::move(consuming_view));

  consuming_view_ptr->RequestFocus();
  ASSERT_EQ(widget()->GetFocusManager()->GetFocusedView(), consuming_view_ptr);

  EnableInputProtection(/*should_block=*/true);

  // Since `SkipDefaultKeyEventProcessingTestView` skips default key processing,
  // arrow keys are treated as action keys rather than focus traversal, so they
  // must be blocked.
  ui::KeyEvent left_arrow(ui::EventType::kKeyPressed, ui::VKEY_LEFT,
                          ui::EF_NONE);
  widget()->OnKeyEvent(&left_arrow);
  EXPECT_TRUE(left_arrow.stopped_propagation());
}

TEST_F(InputProtectionEventHandlerTest, ModifiedArrowKeysBlocked) {
  button()->RequestFocus();
  ASSERT_EQ(widget()->GetFocusManager()->GetFocusedView(), button());

  EnableInputProtection(/*should_block=*/true);

  // Arrow keys with modifiers are not focus traversal keys and must be
  // evaluated and blocked.
  ui::KeyEvent shift_up(ui::EventType::kKeyPressed, ui::VKEY_UP,
                        ui::EF_SHIFT_DOWN);
  widget()->OnKeyEvent(&shift_up);
  EXPECT_TRUE(shift_up.stopped_propagation());

  ui::KeyEvent ctrl_down(ui::EventType::kKeyPressed, ui::VKEY_DOWN,
                         ui::EF_CONTROL_DOWN);
  widget()->OnKeyEvent(&ctrl_down);
  EXPECT_TRUE(ctrl_down.stopped_propagation());

  ui::KeyEvent alt_left(ui::EventType::kKeyPressed, ui::VKEY_LEFT,
                        ui::EF_ALT_DOWN);
  widget()->OnKeyEvent(&alt_left);
  EXPECT_TRUE(alt_left.stopped_propagation());

  ui::KeyEvent altgr_right(ui::EventType::kKeyPressed, ui::VKEY_RIGHT,
                           ui::EF_ALTGR_DOWN);
  widget()->OnKeyEvent(&altgr_right);
  EXPECT_TRUE(altgr_right.stopped_propagation());
}

TEST_F(InputProtectionEventHandlerTest, ModifiedTabKeysBlocked) {
  button()->RequestFocus();
  ASSERT_EQ(widget()->GetFocusManager()->GetFocusedView(), button());

  EnableInputProtection(/*should_block=*/true);

  // Tab keys with Ctrl or Alt are not focus traversal keys and must be
  // evaluated and blocked.
  ui::KeyEvent ctrl_tab(ui::EventType::kKeyPressed, ui::VKEY_TAB,
                        ui::EF_CONTROL_DOWN);
  widget()->OnKeyEvent(&ctrl_tab);
  EXPECT_TRUE(ctrl_tab.stopped_propagation());

  ui::KeyEvent alt_tab(ui::EventType::kKeyPressed, ui::VKEY_TAB,
                       ui::EF_ALT_DOWN);
  widget()->OnKeyEvent(&alt_tab);
  EXPECT_TRUE(alt_tab.stopped_propagation());
}

TEST_F(InputProtectionEventHandlerTest,
       ArrowKeysBypassProtectionWithoutFocusedView) {
  widget()->GetFocusManager()->ClearFocus();
  ASSERT_EQ(widget()->GetFocusManager()->GetFocusedView(), nullptr);

  auto mock_protector =
      std::make_unique<testing::NiceMock<MockInputEventActivationProtector>>();
  auto* mock_protector_ptr = mock_protector.get();

  // Arrow keys are focus traversal keys even when no view is focused and should
  // not trigger input protection checks.
  EXPECT_CALL(*mock_protector_ptr, IsPossiblyUnintendedInteraction(_, _, _))
      .Times(0);
  widget()->EnableInputEventActivationProtection(std::move(mock_protector));

  ui::KeyEvent right_arrow(ui::EventType::kKeyPressed, ui::VKEY_RIGHT,
                           ui::EF_NONE);
  widget()->OnKeyEvent(&right_arrow);
  EXPECT_FALSE(right_arrow.stopped_propagation());
}

TEST_F(InputProtectionEventHandlerTest, ReturnKeyPressBlocked) {
  button()->RequestFocus();
  ASSERT_EQ(widget()->GetFocusManager()->GetFocusedView(), button());

  EnableInputProtection(/*should_block=*/true);

  // Pressing Return on the focused button should be evaluated and blocked.
  ui::KeyEvent return_press(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                            ui::EF_NONE);
  widget()->OnKeyEvent(&return_press);
  EXPECT_TRUE(return_press.stopped_propagation());
  EXPECT_EQ(button_click_count(), 0);
}

TEST_F(InputProtectionEventHandlerTest, AccessibilityModeBypassesProtection) {
  button()->RequestFocus();
  ASSERT_EQ(widget()->GetFocusManager()->GetFocusedView(), button());

  auto mock_protector =
      std::make_unique<testing::NiceMock<MockInputEventActivationProtector>>();
  auto* mock_protector_ptr = mock_protector.get();

  // In accessibility mode, the protector should not be queried.
  EXPECT_CALL(*mock_protector_ptr, IsPossiblyUnintendedInteraction(_, _, _))
      .Times(0);
  widget()->EnableInputEventActivationProtection(std::move(mock_protector));

  // When accessibility is enabled, input protection should be bypassed.
  ui::ScopedAXModeSetter enable_accessibility(ui::AXMode::kNativeAPIs);

  ui::KeyEvent space_press(ui::EventType::kKeyPressed, ui::VKEY_SPACE,
                           ui::EF_NONE);
  widget()->OnKeyEvent(&space_press);

  ui::KeyEvent space_release(ui::EventType::kKeyReleased, ui::VKEY_SPACE,
                             ui::EF_NONE);
  widget()->OnKeyEvent(&space_release);

  EXPECT_EQ(button_click_count(), 1);
}

}  // namespace views::test
