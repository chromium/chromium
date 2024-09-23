// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/button.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/test/ink_drop_host_test_api.h"
#include "ui/views/animation/test/test_ink_drop.h"
#include "ui/views/animation/test/test_ink_drop_host.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/view_metadata_test_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget_utils.h"

#if defined(USE_AURA)
#include "ui/aura/test/test_cursor_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#endif

namespace views {

using test::InkDropHostTestApi;
using test::TestInkDrop;

namespace {

// No-op test double of a ContextMenuController.
class TestContextMenuController : public ContextMenuController {
 public:
  TestContextMenuController() = default;

  TestContextMenuController(const TestContextMenuController&) = delete;
  TestContextMenuController& operator=(const TestContextMenuController&) =
      delete;

  ~TestContextMenuController() override = default;

  // ContextMenuController:
  void ShowContextMenuForViewImpl(View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override {}
};

class TestButton : public Button {
  METADATA_HEADER(TestButton, Button)

 public:
  explicit TestButton(bool has_ink_drop_action_on_click)
      : Button(base::BindRepeating([](bool* pressed) { *pressed = true; },
                                   &pressed_)) {
    SetHasInkDropActionOnClick(has_ink_drop_action_on_click);
  }

  TestButton(const TestButton&) = delete;
  TestButton& operator=(const TestButton&) = delete;

  ~TestButton() override = default;

  KeyClickAction GetKeyClickActionForEvent(const ui::KeyEvent& event) override {
    if (custom_key_click_action_ == KeyClickAction::kNone)
      return Button::GetKeyClickActionForEvent(event);
    return custom_key_click_action_;
  }

  // Button:
  void OnClickCanceled(const ui::Event& event) override { canceled_ = true; }

  bool pressed() const { return pressed_; }
  bool canceled() const { return canceled_; }

  void set_custom_key_click_action(KeyClickAction custom_key_click_action) {
    custom_key_click_action_ = custom_key_click_action;
  }

  void Reset() {
    pressed_ = false;
    canceled_ = false;
  }

  // Raised visibility of OnFocus() to public
  using Button::OnFocus;

 private:
  bool pressed_ = false;
  bool canceled_ = false;

  KeyClickAction custom_key_click_action_ = KeyClickAction::kNone;
};

BEGIN_METADATA(TestButton)
END_METADATA

class TestButtonObserver {
 public:
  explicit TestButtonObserver(Button* button) {
    highlighted_changed_subscription_ =
        InkDrop::Get(button)->AddHighlightedChangedCallback(base::BindRepeating(
            [](TestButtonObserver* obs) { obs->highlighted_changed_ = true; },
            base::Unretained(this)));
    state_changed_subscription_ =
        button->AddStateChangedCallback(base::BindRepeating(
            [](TestButtonObserver* obs) { obs->state_changed_ = true; },
            base::Unretained(this)));
  }

  TestButtonObserver(const TestButtonObserver&) = delete;
  TestButtonObserver& operator=(const TestButtonObserver&) = delete;

  ~TestButtonObserver() = default;

  void Reset() {
    highlighted_changed_ = false;
    state_changed_ = false;
  }

  bool highlighted_changed() const { return highlighted_changed_; }
  bool state_changed() const { return state_changed_; }

 private:
  bool highlighted_changed_ = false;
  bool state_changed_ = false;

  base::CallbackListSubscription highlighted_changed_subscription_;
  base::CallbackListSubscription state_changed_subscription_;
};

TestInkDrop* AddTestInkDrop(TestButton* button) {
  auto owned_ink_drop = std::make_unique<TestInkDrop>();
  TestInkDrop* ink_drop = owned_ink_drop.get();
  InkDrop::Get(button)->SetMode(views::InkDropHost::InkDropMode::ON);
  InkDropHostTestApi(InkDrop::Get(button))
      .SetInkDrop(std::move(owned_ink_drop));
  return ink_drop;
}

}  // namespace

class ButtonTest : public ViewsTestBase {
 public:
  ButtonTest() = default;

  ButtonTest(const ButtonTest&) = delete;
  ButtonTest& operator=(const ButtonTest&) = delete;

  ~ButtonTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();

    // Create a widget so that the Button can query the hover state
    // correctly.
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 650, 650);
    widget_->Init(std::move(params));
    widget_->Show();

    widget()->SetContentsView(std::make_unique<TestButton>(false));

    event_generator_ =
        std::make_unique<ui::test::EventGenerator>(GetRootWindow(widget()));
  }

  void TearDown() override {
    widget_.reset();

    ViewsTestBase::TearDown();
  }

  TestInkDrop* CreateButtonWithInkDrop(bool has_ink_drop_action_on_click) {
    widget()->SetContentsView(
        std::make_unique<TestButton>(has_ink_drop_action_on_click));
    return AddTestInkDrop(button());
  }

  void CreateButtonWithObserver() {
    widget()->SetContentsView(std::make_unique<TestButton>(false));
    InkDrop::Get(button())->SetMode(views::InkDropHost::InkDropMode::ON);
    button_observer_ = std::make_unique<TestButtonObserver>(button());
  }

 protected:
  Widget* widget() { return widget_.get(); }
  TestButton* button() {
    return static_cast<TestButton*>(widget()->GetContentsView());
  }
  TestButtonObserver* button_observer() { return button_observer_.get(); }
  ui::test::EventGenerator* event_generator() { return event_generator_.get(); }
  void SetDraggedView(View* dragged_view) {
    widget_->dragged_view_ = dragged_view;
  }

 private:
  std::unique_ptr<Widget> widget_;
  std::unique_ptr<TestButtonObserver> button_observer_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
};

// Iterate through the metadata for Button to ensure it all works.
TEST_F(ButtonTest, MetadataTest) {
  test::TestViewMetadata(button());
}

// Tests that hover state changes correctly when visibility/enableness changes.
TEST_F(ButtonTest, HoverStateOnVisibilityChange) {
  event_generator()->MoveMouseTo(button()->GetBoundsInScreen().CenterPoint());
  event_generator()->PressLeftButton();
  EXPECT_EQ(Button::STATE_PRESSED, button()->GetState());

  event_generator()->ReleaseLeftButton();
  EXPECT_EQ(Button::STATE_HOVERED, button()->GetState());

  button()->SetEnabled(false);
  EXPECT_EQ(Button::STATE_DISABLED, button()->GetState());

  button()->SetEnabled(true);
  EXPECT_EQ(Button::STATE_HOVERED, button()->GetState());

  button()->SetVisible(false);
  EXPECT_EQ(Button::STATE_NORMAL, button()->GetState());

  button()->SetVisible(true);
  EXPECT_EQ(Button::STATE_HOVERED, button()->GetState());

#if defined(USE_AURA)
  {
    // If another widget has capture, the button should ignore mouse position
    // and not enter hovered state.
    Widget second_widget;
    Widget::InitParams params = CreateParams(
        Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(700, 700, 10, 10);
    second_widget.Init(std::move(params));
    second_widget.Show();
    second_widget.GetNativeWindow()->SetCapture();

    button()->SetEnabled(false);
    EXPECT_EQ(Button::STATE_DISABLED, button()->GetState());

    button()->SetEnabled(true);
    EXPECT_EQ(Button::STATE_NORMAL, button()->GetState());

    button()->SetVisible(false);
    EXPECT_EQ(Button::STATE_NORMAL, button()->GetState());

    button()->SetVisible(true);
    EXPECT_EQ(Button::STATE_NORMAL, button()->GetState());
  }
#endif

// Disabling cursor events occurs for touch events and the Ash magnifier. There
// is no touch on desktop Mac. Tracked in http://crbug.com/445520.
#if !BUILDFLAG(IS_MAC) || defined(USE_AURA)
  aura::test::TestCursorClient cursor_client(GetRootWindow(widget()));

  // In Aura views, no new hover effects are invoked if mouse events
  // are disabled.
  cursor_client.DisableMouseEvents();

  button()->SetEnabled(false);
  EXPECT_EQ(Button::STATE_DISABLED, button()->GetState());

  button()->SetEnabled(true);
  EXPECT_EQ(Button::STATE_NORMAL, button()->GetState());

  button()->SetVisible(false);
  EXPECT_EQ(Button::STATE_NORMAL, button()->GetState());

  button()->SetVisible(true);
  EXPECT_EQ(Button::STATE_NORMAL, button()->GetState());
#endif  // !BUILDFLAG(IS_MAC) || defined(USE_AURA)
}

// Tests that the hover state is preserved during a view hierarchy update of a
// button's child View.
TEST_F(ButtonTest, HoverStatePreservedOnDescendantViewHierarchyChange) {
  event_generator()->MoveMouseTo(button()->GetBoundsInScreen().CenterPoint());

  EXPECT_EQ(Button::STATE_HOVERED, button()->GetState());
  Label* child = new Label(std::u16string());
  button()->AddChildView(child);
  delete child;
  EXPECT_EQ(Button::STATE_HOVERED, button()->GetState());
}

TEST_F(ButtonTest, AccessibleHoveredStateUpdatesCorrectly) {
  event_generator()->MoveMouseTo(button()->GetBoundsInScreen().CenterPoint());
  event_generator()->PressLeftButton();

  ui::AXNodeData node_data;
  button()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_FALSE(button()->GetViewAccessibility().GetIsHovered());
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kHovered));

  event_generator()->ReleaseLeftButton();
  node_data = ui::AXNodeData();
  button()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(button()->GetViewAccessibility().GetIsHovered());
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kHovered));

  button()->SetEnabled(false);
  EXPECT_EQ(Button::STATE_DISABLED, button()->GetState());
  node_data = ui::AXNodeData();
  button()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_FALSE(button()->GetViewAccessibility().GetIsHovered());
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kHovered));

  button()->SetEnabled(true);
  node_data = ui::AXNodeData();
  button()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(button()->GetViewAccessibility().GetIsHovered());
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kHovered));

  button()->SetVisible(false);
  EXPECT_EQ(Button::STATE_NORMAL, button()->GetState());
  node_data = ui::AXNodeData();
  button()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_FALSE(button()->GetViewAccessibility().GetIsHovered());
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kHovered));

  button()->SetVisible(true);
  node_data = ui::AXNodeData();
  button()->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(button()->GetViewAccessibility().GetIsHovered());
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kHovered));
}

// Tests the different types of NotifyActions.
TEST_F(ButtonTest, NotifyAction) {
  gfx::Point center(10, 10);

  // By default the button should notify the callback on mouse release.
  button()->OnMousePressed(ui::MouseEvent(
      ui::EventType::kMousePressed, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(Button::STATE_PRESSED, button()->GetState());
  EXPECT_FALSE(button()->pressed());

  button()->OnMouseReleased(ui::MouseEvent(
      ui::EventType::kMouseReleased, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(Button::STATE_HOVERED, button()->GetState());
  EXPECT_TRUE(button()->pressed());

  // Set the notify action to happen on mouse press.
  button()->Reset();
  button()->button_controller()->set_notify_action(
      ButtonController::NotifyAction::kOnPress);
  button()->OnMousePressed(ui::MouseEvent(
      ui::EventType::kMousePressed, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(Button::STATE_PRESSED, button()->GetState());
  EXPECT_TRUE(button()->pressed());

  // The button should no longer notify on mouse release.
  button()->Reset();
  button()->OnMouseReleased(ui::MouseEvent(
      ui::EventType::kMouseReleased, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(Button::STATE_HOVERED, button()->GetState());
  EXPECT_FALSE(button()->pressed());
}

// Tests that OnClickCanceled gets called when NotifyClick is not expected
// anymore.
TEST_F(ButtonTest, NotifyActionNoClick) {
  gfx::Point center(10, 10);

  // By default the button should notify the callback on mouse release.
  button()->OnMousePressed(ui::MouseEvent(
      ui::EventType::kMousePressed, center, center, ui::EventTimeForNow(),
      ui::EF_RIGHT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON));
  EXPECT_FALSE(button()->canceled());

  button()->OnMouseReleased(ui::MouseEvent(
      ui::EventType::kMouseReleased, center, center, ui::EventTimeForNow(),
      ui::EF_RIGHT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON));
  EXPECT_TRUE(button()->canceled());

  // Set the notify action to happen on mouse press.
  button()->Reset();
  button()->button_controller()->set_notify_action(
      ButtonController::NotifyAction::kOnPress);
  button()->OnMousePressed(ui::MouseEvent(
      ui::EventType::kMousePressed, center, center, ui::EventTimeForNow(),
      ui::EF_RIGHT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON));
  // OnClickCanceled is only sent on mouse release.
  EXPECT_FALSE(button()->canceled());

  // The button should no longer notify on mouse release.
  button()->Reset();
  button()->OnMouseReleased(ui::MouseEvent(
      ui::EventType::kMouseReleased, center, center, ui::EventTimeForNow(),
      ui::EF_RIGHT_MOUSE_BUTTON, ui::EF_RIGHT_MOUSE_BUTTON));
  EXPECT_FALSE(button()->canceled());
}

TEST_F(ButtonTest, ButtonControllerNotifyClick) {
  EXPECT_FALSE(button()->pressed());
  button()->button_controller()->NotifyClick();
  EXPECT_TRUE(button()->pressed());
}

// No touch on desktop Mac. Tracked in http://crbug.com/445520.
#if !BUILDFLAG(IS_MAC) || defined(USE_AURA)

namespace {

void PerformGesture(Button* button, ui::EventType event_type) {
  ui::GestureEventDetails gesture_details(event_type);
  ui::GestureEvent gesture_event(0, 0, 0, base::TimeTicks(), gesture_details);
  button->OnGestureEvent(&gesture_event);
}

}  // namespace

// Tests that gesture events correctly change the button state.
TEST_F(ButtonTest, GestureEventsSetState) {
  aura::test::TestCursorClient cursor_client(GetRootWindow(widget()));

  EXPECT_EQ(Button::STATE_NORMAL, button()->GetState());

  PerformGesture(button(), ui::EventType::kGestureTapDown);
  EXPECT_EQ(Button::STATE_PRESSED, button()->GetState());

  PerformGesture(button(), ui::EventType::kGestureShowPress);
  EXPECT_EQ(Button::STATE_PRESSED, button()->GetState());

  PerformGesture(button(), ui::EventType::kGestureTapCancel);
  EXPECT_EQ(Button::STATE_NORMAL, button()->GetState());
}

// Tests that if the button was disabled in its button press handler, gesture
// events will not revert the disabled state back to normal.
// https://crbug.com/1084241.
TEST_F(ButtonTest, GestureEventsRespectDisabledState) {
  button()->SetCallback(base::BindRepeating(
      [](TestButton* button) { button->SetEnabled(false); }, button()));

  EXPECT_EQ(Button::STATE_NORMAL, button()->GetState());
  event_generator()->GestureTapAt(button()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(Button::STATE_DISABLED, button()->GetState());
}

#endif  // !BUILDFLAG(IS_MAC) || defined(USE_AURA)

// Ensure subclasses of Button are correctly recognized as Button.
TEST_F(ButtonTest, AsButton) {
  std::u16string text;

  LabelButton label_button(Button::PressedCallback(), text);
  EXPECT_TRUE(Button::AsButton(&label_button));

  ImageButton image_button;
  EXPECT_TRUE(Button::AsButton(&image_button));

  Checkbox checkbox(text);
  EXPECT_TRUE(Button::AsButton(&checkbox));

  RadioButton radio_button(text, 0);
  EXPECT_TRUE(Button::AsButton(&radio_button));

  MenuButton menu_button(Button::PressedCallback(), text);
  EXPECT_TRUE(Button::AsButton(&menu_button));

  ToggleButton toggle_button;
  EXPECT_TRUE(Button::AsButton(&toggle_button));

  Label label;
  EXPECT_FALSE(Button::AsButton(&label));

  Link link;
  EXPECT_FALSE(Button::AsButton(&link));

  Textfield textfield;
  EXPECT_FALSE(Button::AsButton(&textfield));
}

// Tests that pressing a button shows the ink drop and releasing the button
// does not hide the ink drop.
// Note: Ink drop is not hidden upon release because Button descendants
// may enter a different ink drop state.
TEST_F(ButtonTest, ButtonClickTogglesInkDrop) {
  TestInkDrop* ink_drop = CreateButtonWithInkDrop(false);

  event_generator()->MoveMouseTo(button()->GetBoundsInScreen().CenterPoint());
  event_generator()->PressLeftButton();
  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());

  event_generator()->ReleaseLeftButton();
  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());
}

// Tests that pressing a button shows and releasing capture hides ink drop.
// Releasing capture should also reset PRESSED button state to NORMAL.
TEST_F(ButtonTest, CaptureLossHidesInkDrop) {
  TestInkDrop* ink_drop = CreateButtonWithInkDrop(false);

  event_generator()->MoveMouseTo(button()->GetBoundsInScreen().CenterPoint());
  event_generator()->PressLeftButton();
  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());

  EXPECT_EQ(Button::ButtonState::STATE_PRESSED, button()->GetState());
  SetDraggedView(button());
  widget()->SetCapture(button());
  widget()->ReleaseCapture();
  SetDraggedView(nullptr);
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());
  EXPECT_EQ(Button::ButtonState::STATE_NORMAL, button()->GetState());
}

TEST_F(ButtonTest, HideInkDropWhenShowingContextMenu) {
  TestInkDrop* ink_drop = CreateButtonWithInkDrop(false);
  TestContextMenuController context_menu_controller;
  button()->set_context_menu_controller(&context_menu_controller);
  button()->SetHideInkDropWhenShowingContextMenu(true);

  ink_drop->SetHovered(true);
  ink_drop->AnimateToState(InkDropState::ACTION_PENDING);

  button()->ShowContextMenu(gfx::Point(), ui::MENU_SOURCE_MOUSE);

  EXPECT_FALSE(ink_drop->is_hovered());
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());
}

TEST_F(ButtonTest, DontHideInkDropWhenShowingContextMenu) {
  TestInkDrop* ink_drop = CreateButtonWithInkDrop(false);
  TestContextMenuController context_menu_controller;
  button()->set_context_menu_controller(&context_menu_controller);
  button()->SetHideInkDropWhenShowingContextMenu(false);

  ink_drop->SetHovered(true);
  ink_drop->AnimateToState(InkDropState::ACTION_PENDING);

  button()->ShowContextMenu(gfx::Point(), ui::MENU_SOURCE_MOUSE);

  EXPECT_TRUE(ink_drop->is_hovered());
  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());
}

TEST_F(ButtonTest, HideInkDropOnBlur) {
  gfx::Point center(10, 10);

  TestInkDrop* ink_drop = CreateButtonWithInkDrop(false);

  button()->OnFocus();

  button()->OnMousePressed(ui::MouseEvent(
      ui::EventType::kMousePressed, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());

  button()->OnBlur();
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());

  button()->OnMouseReleased(ui::MouseEvent(
      ui::EventType::kMousePressed, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_TRUE(button()->pressed());
}

TEST_F(ButtonTest, HideInkDropHighlightOnDisable) {
  TestInkDrop* ink_drop = CreateButtonWithInkDrop(false);

  event_generator()->MoveMouseTo(button()->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(ink_drop->is_hovered());
  button()->SetEnabled(false);
  EXPECT_FALSE(ink_drop->is_hovered());
  button()->SetEnabled(true);
  EXPECT_TRUE(ink_drop->is_hovered());
}

TEST_F(ButtonTest, InkDropAfterTryingToShowContextMenu) {
  TestInkDrop* ink_drop = CreateButtonWithInkDrop(false);
  button()->set_context_menu_controller(nullptr);

  ink_drop->SetHovered(true);
  ink_drop->AnimateToState(InkDropState::ACTION_PENDING);

  button()->ShowContextMenu(gfx::Point(), ui::MENU_SOURCE_MOUSE);

  EXPECT_TRUE(ink_drop->is_hovered());
  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());
}

TEST_F(ButtonTest, HideInkDropHighlightWhenRemoved) {
  View* contents_view = widget()->SetContentsView(std::make_unique<View>());

  TestButton* button =
      contents_view->AddChildView(std::make_unique<TestButton>(false));
  button->SetBounds(0, 0, 200, 200);
  TestInkDrop* ink_drop = AddTestInkDrop(button);

  // Make sure that the button ink drop is hidden after the button gets removed.
  event_generator()->MoveMouseTo(button->GetBoundsInScreen().origin());
  event_generator()->MoveMouseBy(2, 2);
  EXPECT_TRUE(ink_drop->is_hovered());
  // Set ink-drop state to ACTIVATED to make sure that removing the container
  // sets it back to HIDDEN.
  ink_drop->AnimateToState(InkDropState::ACTIVATED);
  auto owned_button = contents_view->RemoveChildViewT(button);
  button = nullptr;

  EXPECT_FALSE(ink_drop->is_hovered());
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());

  // Make sure hiding the ink drop happens even if the button is indirectly
  // being removed.
  View* parent_view = contents_view->AddChildView(std::make_unique<View>());
  parent_view->SetBounds(0, 0, 400, 400);
  button = parent_view->AddChildView(std::move(owned_button));

  // Trigger hovering and then remove from the indirect parent. This should
  // propagate down to Button which should remove the highlight effect.
  EXPECT_FALSE(ink_drop->is_hovered());
  event_generator()->MoveMouseBy(8, 8);
  EXPECT_TRUE(ink_drop->is_hovered());
  // Set ink-drop state to ACTIVATED to make sure that removing the container
  // sets it back to HIDDEN.
  ink_drop->AnimateToState(InkDropState::ACTIVATED);
  auto owned_parent = contents_view->RemoveChildViewT(parent_view);
  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());
  EXPECT_FALSE(ink_drop->is_hovered());
}

// Tests that when button is set to notify on release, dragging mouse out and
// back transitions ink drop states correctly.
TEST_F(ButtonTest, InkDropShowHideOnMouseDraggedNotifyOnRelease) {
  gfx::Point center(10, 10);
  gfx::Point oob(-1, -1);

  TestInkDrop* ink_drop = CreateButtonWithInkDrop(false);
  button()->button_controller()->set_notify_action(
      ButtonController::NotifyAction::kOnRelease);

  button()->OnMousePressed(ui::MouseEvent(
      ui::EventType::kMousePressed, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());

  button()->OnMouseDragged(ui::MouseEvent(
      ui::EventType::kMousePressed, oob, oob, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());

  button()->OnMouseDragged(ui::MouseEvent(
      ui::EventType::kMousePressed, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());

  button()->OnMouseDragged(ui::MouseEvent(
      ui::EventType::kMousePressed, oob, oob, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());

  button()->OnMouseReleased(ui::MouseEvent(
      ui::EventType::kMousePressed, oob, oob, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_FALSE(button()->pressed());
}

// Tests that when button is set to notify on press, dragging mouse out and back
// does not change the ink drop state.
TEST_F(ButtonTest, InkDropShowHideOnMouseDraggedNotifyOnPress) {
  gfx::Point center(10, 10);
  gfx::Point oob(-1, -1);

  TestInkDrop* ink_drop = CreateButtonWithInkDrop(true);
  button()->button_controller()->set_notify_action(
      ButtonController::NotifyAction::kOnPress);

  button()->OnMousePressed(ui::MouseEvent(
      ui::EventType::kMousePressed, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::ACTION_TRIGGERED, ink_drop->GetTargetInkDropState());
  EXPECT_TRUE(button()->pressed());

  button()->OnMouseDragged(ui::MouseEvent(
      ui::EventType::kMousePressed, oob, oob, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::ACTION_TRIGGERED, ink_drop->GetTargetInkDropState());

  button()->OnMouseDragged(ui::MouseEvent(
      ui::EventType::kMousePressed, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::ACTION_TRIGGERED, ink_drop->GetTargetInkDropState());

  button()->OnMouseDragged(ui::MouseEvent(
      ui::EventType::kMousePressed, oob, oob, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::ACTION_TRIGGERED, ink_drop->GetTargetInkDropState());

  button()->OnMouseReleased(ui::MouseEvent(
      ui::EventType::kMousePressed, oob, oob, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::ACTION_TRIGGERED, ink_drop->GetTargetInkDropState());
}

TEST_F(ButtonTest, InkDropStaysHiddenWhileDragging) {
  gfx::Point center(10, 10);
  gfx::Point oob(-1, -1);

  TestInkDrop* ink_drop = CreateButtonWithInkDrop(false);

  button()->OnMousePressed(ui::MouseEvent(
      ui::EventType::kMousePressed, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop->GetTargetInkDropState());

  SetDraggedView(button());
  widget()->SetCapture(button());
  widget()->ReleaseCapture();

  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());

  button()->OnMouseDragged(ui::MouseEvent(
      ui::EventType::kMousePressed, oob, oob, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());

  button()->OnMouseDragged(ui::MouseEvent(
      ui::EventType::kMousePressed, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));

  EXPECT_EQ(InkDropState::HIDDEN, ink_drop->GetTargetInkDropState());

  SetDraggedView(nullptr);
}

// Ensure PressedCallback is dynamically settable.
TEST_F(ButtonTest, SetCallback) {
  bool pressed = false;
  button()->SetCallback(
      base::BindRepeating([](bool* pressed) { *pressed = true; }, &pressed));

  const gfx::Point center(10, 10);
  button()->OnMousePressed(ui::MouseEvent(
      ui::EventType::kMousePressed, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  // Default button controller notifies callback at mouse release.
  button()->OnMouseReleased(ui::MouseEvent(
      ui::EventType::kMouseReleased, center, center, ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON));
  EXPECT_TRUE(pressed);
}

// VisibilityTestButton tests to see if an ink drop or a layer has been added to
// the button at any point during the visibility state changes of its Widget.
class VisibilityTestButton : public TestButton {
 public:
  VisibilityTestButton() : TestButton(false) {}
  ~VisibilityTestButton() override {
    if (layer())
      ADD_FAILURE();
  }

  void AddLayerToRegion(ui::Layer* layer, views::LayerRegion region) override {
    ADD_FAILURE();
  }

  void RemoveLayerFromRegions(ui::Layer* layer) override { ADD_FAILURE(); }
};

// Test that hiding or closing a Widget doesn't attempt to add a layer due to
// changed visibility states.
TEST_F(ButtonTest, NoLayerAddedForWidgetVisibilityChanges) {
  VisibilityTestButton* button =
      widget()->SetContentsView(std::make_unique<VisibilityTestButton>());

  // Ensure no layers are created during construction.
  EXPECT_TRUE(button->GetVisible());
  EXPECT_FALSE(button->layer());

  // Ensure no layers are created when hiding the widget.
  widget()->Hide();
  EXPECT_FALSE(button->layer());

  // Ensure no layers are created when the widget is reshown.
  widget()->Show();
  EXPECT_FALSE(button->layer());

  // Ensure no layers are created during the closing of the Widget.
  widget()->Close();  // Start an asynchronous close.
  EXPECT_FALSE(button->layer());

  // Ensure no layers are created following the Widget's destruction.
  base::RunLoop().RunUntilIdle();  // Complete the Close().
}

// Verify that the Space key clicks the button on key-press on Mac, and
// key-release on other platforms.
TEST_F(ButtonTest, ActionOnSpace) {
  // Give focus to the button.
  button()->RequestFocus();
  EXPECT_TRUE(button()->HasFocus());

  ui::KeyEvent space_press(ui::EventType::kKeyPressed, ui::VKEY_SPACE,
                           ui::EF_NONE);
  EXPECT_TRUE(button()->OnKeyPressed(space_press));

#if BUILDFLAG(IS_MAC)
  EXPECT_EQ(Button::STATE_NORMAL, button()->GetState());
  EXPECT_TRUE(button()->pressed());
#else
  EXPECT_EQ(Button::STATE_PRESSED, button()->GetState());
  EXPECT_FALSE(button()->pressed());
#endif

  ui::KeyEvent space_release(ui::EventType::kKeyReleased, ui::VKEY_SPACE,
                             ui::EF_NONE);

#if BUILDFLAG(IS_MAC)
  EXPECT_FALSE(button()->OnKeyReleased(space_release));
#else
  EXPECT_TRUE(button()->OnKeyReleased(space_release));
#endif

  EXPECT_EQ(Button::STATE_NORMAL, button()->GetState());
  EXPECT_TRUE(button()->pressed());
}

// Verify that the Return key clicks the button on key-press on all platforms
// except Mac. On Mac, the Return key performs the default action associated
// with a dialog, even if a button has focus.
TEST_F(ButtonTest, ActionOnReturn) {
  // Give focus to the button.
  button()->RequestFocus();
  EXPECT_TRUE(button()->HasFocus());

  ui::KeyEvent return_press(ui::EventType::kKeyPressed, ui::VKEY_RETURN,
                            ui::EF_NONE);

#if BUILDFLAG(IS_MAC)
  EXPECT_FALSE(button()->OnKeyPressed(return_press));
  EXPECT_EQ(Button::STATE_NORMAL, button()->GetState());
  EXPECT_FALSE(button()->pressed());
#else
  EXPECT_TRUE(button()->OnKeyPressed(return_press));
  EXPECT_EQ(Button::STATE_NORMAL, button()->GetState());
  EXPECT_TRUE(button()->pressed());
#endif

  ui::KeyEvent return_release(ui::EventType::kKeyReleased, ui::VKEY_RETURN,
                              ui::EF_NONE);
  EXPECT_FALSE(button()->OnKeyReleased(return_release));
}

// Verify that a subclass may customize the action for a key pressed event.
TEST_F(ButtonTest, CustomActionOnKeyPressedEvent) {
  // Give focus to the button.
  button()->RequestFocus();
  EXPECT_TRUE(button()->HasFocus());

  // Set the button to handle any key pressed event as kOnKeyPress.
  button()->set_custom_key_click_action(Button::KeyClickAction::kOnKeyPress);

  ui::KeyEvent control_press(ui::EventType::kKeyPressed, ui::VKEY_CONTROL,
                             ui::EF_NONE);
  EXPECT_TRUE(button()->OnKeyPressed(control_press));
  EXPECT_EQ(Button::STATE_NORMAL, button()->GetState());
  EXPECT_TRUE(button()->pressed());

  ui::KeyEvent control_release(ui::EventType::kKeyReleased, ui::VKEY_CONTROL,
                               ui::EF_NONE);
  EXPECT_FALSE(button()->OnKeyReleased(control_release));
}

// Verifies that button activation highlight state changes trigger property
// change callbacks.
TEST_F(ButtonTest, ChangingHighlightStateNotifiesCallback) {
  CreateButtonWithObserver();
  EXPECT_FALSE(button_observer()->highlighted_changed());
  EXPECT_FALSE(InkDrop::Get(button())->GetHighlighted());

  button()->SetHighlighted(/*highlighted=*/true);
  EXPECT_TRUE(button_observer()->highlighted_changed());
  EXPECT_TRUE(InkDrop::Get(button())->GetHighlighted());

  button_observer()->Reset();
  EXPECT_FALSE(button_observer()->highlighted_changed());
  EXPECT_TRUE(InkDrop::Get(button())->GetHighlighted());

  button()->SetHighlighted(/*highlighted=*/false);
  EXPECT_TRUE(button_observer()->highlighted_changed());
  EXPECT_FALSE(InkDrop::Get(button())->GetHighlighted());
}

// Verifies that button state changes trigger property change callbacks.
TEST_F(ButtonTest, ClickingButtonNotifiesObserverOfStateChanges) {
  CreateButtonWithObserver();
  EXPECT_FALSE(button_observer()->state_changed());
  EXPECT_EQ(Button::STATE_NORMAL, button()->GetState());

  event_generator()->MoveMouseTo(button()->GetBoundsInScreen().CenterPoint());
  event_generator()->PressLeftButton();
  EXPECT_TRUE(button_observer()->state_changed());
  EXPECT_EQ(Button::STATE_PRESSED, button()->GetState());

  button_observer()->Reset();
  EXPECT_FALSE(button_observer()->state_changed());
  EXPECT_EQ(Button::STATE_PRESSED, button()->GetState());

  event_generator()->ReleaseLeftButton();
  EXPECT_TRUE(button_observer()->state_changed());
  EXPECT_EQ(Button::STATE_HOVERED, button()->GetState());
}

// Verifies that direct calls to Button::SetState() trigger property change
// callbacks.
TEST_F(ButtonTest, SetStateNotifiesObserver) {
  CreateButtonWithObserver();
  EXPECT_FALSE(button_observer()->state_changed());
  EXPECT_EQ(Button::STATE_NORMAL, button()->GetState());

  button()->SetState(Button::STATE_HOVERED);
  EXPECT_TRUE(button_observer()->state_changed());
  EXPECT_EQ(Button::STATE_HOVERED, button()->GetState());

  button_observer()->Reset();
  EXPECT_FALSE(button_observer()->state_changed());
  EXPECT_EQ(Button::STATE_HOVERED, button()->GetState());

  button()->SetState(Button::STATE_NORMAL);
  EXPECT_TRUE(button_observer()->state_changed());
  EXPECT_EQ(Button::STATE_NORMAL, button()->GetState());
}

// Verifies setting the tooltip text will call NotifyAccessibilityEvent.
TEST_F(ButtonTest, SetTooltipTextNotifiesAccessibilityEvent) {
  std::u16string test_tooltip_text = u"Test Tooltip Text";
  test::AXEventCounter counter(views::AXEventManager::Get());
  EXPECT_EQ(0, counter.GetCount(ax::mojom::Event::kTextChanged));
  button()->SetTooltipText(test_tooltip_text);
  EXPECT_EQ(1, counter.GetCount(ax::mojom::Event::kTextChanged));
  EXPECT_EQ(test_tooltip_text, button()->GetTooltipText(gfx::Point()));
  ui::AXNodeData data;
  button()->GetViewAccessibility().GetAccessibleNodeData(&data);
  const std::string& name =
      data.GetStringAttribute(ax::mojom::StringAttribute::kName);
  EXPECT_EQ(test_tooltip_text, base::ASCIIToUTF16(name));
}

TEST_F(ButtonTest, AccessibleRole) {
  ui::AXNodeData data;
  button()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kButton);
  EXPECT_EQ(button()->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kButton);

  button()->GetViewAccessibility().SetRole(ax::mojom::Role::kCheckBox);

  data = ui::AXNodeData();
  button()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kCheckBox);
  EXPECT_EQ(button()->GetViewAccessibility().GetCachedRole(),
            ax::mojom::Role::kCheckBox);
}

TEST_F(ButtonTest, AccessibleCheckedState) {
  ui::AXNodeData data;
  event_generator()->MoveMouseTo(button()->GetBoundsInScreen().CenterPoint());
  event_generator()->PressLeftButton();
  EXPECT_EQ(Button::STATE_PRESSED, button()->GetState());
  button()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kTrue);

  event_generator()->ReleaseLeftButton();
  EXPECT_EQ(Button::STATE_HOVERED, button()->GetState());
  data = ui::AXNodeData();
  button()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetCheckedState(), ax::mojom::CheckedState::kNone);
}

TEST_F(ButtonTest, AccessibleDefaultActionVerb) {
  ui::AXNodeData data;
  button()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetDefaultActionVerb(), ax::mojom::DefaultActionVerb::kPress);

  data = ui::AXNodeData();
  button()->SetEnabled(false);
  button()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_FALSE(
      data.HasIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb));

  data = ui::AXNodeData();
  button()->SetEnabled(true);
  button()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetDefaultActionVerb(), ax::mojom::DefaultActionVerb::kPress);
}

TEST_F(ButtonTest, AnchorHighlightSetsHiglight) {
  TestInkDrop* ink_drop = CreateButtonWithInkDrop(false);

  Button::ScopedAnchorHighlight highlight = button()->AddAnchorHighlight();
  EXPECT_EQ(InkDropState::ACTIVATED, ink_drop->GetTargetInkDropState());
}

TEST_F(ButtonTest, AnchorHighlightDestructionClearsHighlight) {
  TestInkDrop* ink_drop = CreateButtonWithInkDrop(false);

  std::optional<Button::ScopedAnchorHighlight> highlight =
      button()->AddAnchorHighlight();
  EXPECT_EQ(InkDropState::ACTIVATED, ink_drop->GetTargetInkDropState());

  highlight.reset();
  EXPECT_EQ(InkDropState::DEACTIVATED, ink_drop->GetTargetInkDropState());
}

TEST_F(ButtonTest, NestedAnchorHighlights) {
  TestInkDrop* ink_drop = CreateButtonWithInkDrop(false);

  std::optional<Button::ScopedAnchorHighlight> highlight1 =
      button()->AddAnchorHighlight();
  std::optional<Button::ScopedAnchorHighlight> highlight2 =
      button()->AddAnchorHighlight();

  EXPECT_EQ(InkDropState::ACTIVATED, ink_drop->GetTargetInkDropState());

  highlight2.reset();
  EXPECT_EQ(InkDropState::ACTIVATED, ink_drop->GetTargetInkDropState());

  highlight1.reset();
  EXPECT_EQ(InkDropState::DEACTIVATED, ink_drop->GetTargetInkDropState());
}

TEST_F(ButtonTest, OverlappingAnchorHighlights) {
  TestInkDrop* ink_drop = CreateButtonWithInkDrop(false);

  std::optional<Button::ScopedAnchorHighlight> highlight1 =
      button()->AddAnchorHighlight();
  std::optional<Button::ScopedAnchorHighlight> highlight2 =
      button()->AddAnchorHighlight();

  EXPECT_EQ(InkDropState::ACTIVATED, ink_drop->GetTargetInkDropState());

  highlight1.reset();
  EXPECT_EQ(InkDropState::ACTIVATED, ink_drop->GetTargetInkDropState());

  highlight2.reset();
  EXPECT_EQ(InkDropState::DEACTIVATED, ink_drop->GetTargetInkDropState());
}

TEST_F(ButtonTest, AnchorHighlightsCanOutliveButton) {
  CreateButtonWithInkDrop(false);

  std::optional<Button::ScopedAnchorHighlight> highlight =
      button()->AddAnchorHighlight();

  // Creating a new button will destroy the old one
  CreateButtonWithInkDrop(false);

  highlight.reset();
}

using ButtonActionViewInterfaceTest = ButtonTest;

TEST_F(ButtonActionViewInterfaceTest, TestActionChanged) {
  const std::u16string test_string = u"test_string";
  std::unique_ptr<actions::ActionItem> action_item =
      actions::ActionItem::Builder()
          .SetTooltipText(test_string)
          .SetActionId(0)
          .SetEnabled(false)
          .Build();
  button()->GetActionViewInterface()->ActionItemChangedImpl(action_item.get());
  // Test some properties to ensure that the right ActionViewInterface is linked
  // to the view.
  EXPECT_EQ(test_string, button()->GetTooltipText());
  EXPECT_FALSE(button()->GetEnabled());
}

TEST_F(ButtonActionViewInterfaceTest, TestActionTriggered) {
  ui::MouseEvent e(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), 0, 0);
  bool called = false;
  button()->GetActionViewInterface()->LinkActionInvocationToView(
      base::BindRepeating([](bool* called_bool) { *called_bool = true; },
                          &called));
  views::test::ButtonTestApi test_api(button());
  test_api.NotifyClick(e);
  EXPECT_TRUE(called);
}

}  // namespace views
