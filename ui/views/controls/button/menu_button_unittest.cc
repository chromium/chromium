// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/button/menu_button.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/test/ink_drop_host_test_api.h"
#include "ui/views/animation/test/test_ink_drop.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/drag_controller.h"
#include "ui/views/style/platform_style.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget_utils.h"

#if defined(USE_AURA)
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_client_observer.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#endif

namespace views {

using ::base::ASCIIToUTF16;
using ::ui::mojom::DragOperation;

class TestMenuButton : public MenuButton {
 public:
  TestMenuButton()
      : TestMenuButton(base::BindRepeating(&TestMenuButton::ButtonPressed,
                                           base::Unretained(this))) {}
  explicit TestMenuButton(PressedCallback callback)
      : MenuButton(std::move(callback), std::u16string(u"button")) {}
  TestMenuButton(const TestMenuButton&) = delete;
  TestMenuButton& operator=(const TestMenuButton&) = delete;
  ~TestMenuButton() override = default;

  bool clicked() const { return clicked_; }
  Button::ButtonState last_state() const { return last_state_; }
  ui::EventType last_event_type() const { return last_event_type_; }

  void Reset() {
    clicked_ = false;
    last_state_ = Button::STATE_NORMAL;
    last_event_type_ = ui::EventType::kUnknown;
  }

 private:
  void ButtonPressed(const ui::Event& event) {
    clicked_ = true;
    last_state_ = GetState();
    last_event_type_ = event.type();
  }

  bool clicked_ = false;
  Button::ButtonState last_state_ = Button::STATE_NORMAL;
  ui::EventType last_event_type_ = ui::EventType::kUnknown;
};

class MenuButtonTest : public ViewsTestBase {
 public:
  MenuButtonTest() = default;
  MenuButtonTest(const MenuButtonTest&) = delete;
  MenuButtonTest& operator=(const MenuButtonTest&) = delete;
  ~MenuButtonTest() override = default;

  void TearDown() override {
    generator_.reset();
    widget_.reset();
    ViewsTestBase::TearDown();
  }

 protected:
  Widget* widget() { return widget_.get(); }
  TestMenuButton* button() {
    return static_cast<TestMenuButton*>(widget()->GetContentsView());
  }
  ui::test::EventGenerator* generator() { return generator_.get(); }
  test::TestInkDrop* ink_drop() {
    return static_cast<test::TestInkDrop*>(
        test::InkDropHostTestApi(InkDrop::Get(button())).ink_drop());
  }

  gfx::Point GetOutOfButtonLocation() {
    return gfx::Point(button()->x() - 1, button()->y() - 1);
  }

  void ConfigureMenuButton(std::unique_ptr<TestMenuButton> test_button) {
    CHECK(!widget_);

    widget_ = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    widget_->Init(std::move(params));
    widget_->Show();

    generator_ =
        std::make_unique<ui::test::EventGenerator>(GetRootWindow(widget()));
    // Set initial mouse location in a consistent way so that the menu button we
    // are about to create initializes its hover state in a consistent manner.
    generator_->set_current_screen_location(gfx::Point(10, 10));

    widget_->SetContentsView(std::move(test_button));
    button()->SetBoundsRect(gfx::Rect(0, 0, 200, 20));

    auto ink_drop = std::make_unique<test::TestInkDrop>();
    test::InkDropHostTestApi(InkDrop::Get(button()))
        .SetInkDrop(std::move(ink_drop));

    widget_->Show();
  }

 private:
  std::unique_ptr<Widget> widget_;
  std::unique_ptr<ui::test::EventGenerator> generator_;
};

// A Button that will acquire a PressedLock in the pressed callback and
// optionally release it as well.
class PressStateButton : public TestMenuButton {
 public:
  explicit PressStateButton(bool release_lock)
      : TestMenuButton(base::BindRepeating(&PressStateButton::ButtonPressed,
                                           base::Unretained(this))),
        release_lock_(release_lock) {}
  PressStateButton(const PressStateButton&) = delete;
  PressStateButton& operator=(const PressStateButton&) = delete;
  ~PressStateButton() override = default;

  void ReleasePressedLock() { pressed_lock_.reset(); }

 private:
  void ButtonPressed() {
    pressed_lock_ = button_controller()->TakeLock();
    if (release_lock_)
      ReleasePressedLock();
  }

  bool release_lock_;
  std::unique_ptr<MenuButtonController::PressedLock> pressed_lock_;
};

// Basic implementation of a DragController, to test input behaviour for
// MenuButtons that can be dragged.
class TestDragController : public DragController {
 public:
  TestDragController() = default;
  TestDragController(const TestDragController&) = delete;
  TestDragController& operator=(const TestDragController&) = delete;
  ~TestDragController() override = default;

  void WriteDragDataForView(View* sender,
                            const gfx::Point& press_pt,
                            ui::OSExchangeData* data) override {}

  int GetDragOperationsForView(View* sender, const gfx::Point& p) override {
    return ui::DragDropTypes::DRAG_MOVE;
  }

  bool CanStartDragForView(View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& p) override {
    return true;
  }
};

#if defined(USE_AURA)
// Basic implementation of a DragDropClient, tracking the state of the drag
// operation. While dragging addition mouse events are consumed, preventing the
// target view from receiving them.
class TestDragDropClient : public aura::client::DragDropClient,
                           public ui::EventHandler {
 public:
  TestDragDropClient();
  TestDragDropClient(const TestDragDropClient&) = delete;
  TestDragDropClient& operator=(const TestDragDropClient&) = delete;
  ~TestDragDropClient() override;

  // aura::client::DragDropClient:
  DragOperation StartDragAndDrop(std::unique_ptr<ui::OSExchangeData> data,
                                 aura::Window* root_window,
                                 aura::Window* source_window,
                                 const gfx::Point& screen_location,
                                 int allowed_operations,
                                 ui::mojom::DragEventSource source) override;
#if BUILDFLAG(IS_LINUX)
  void UpdateDragImage(const gfx::ImageSkia& image,
                       const gfx::Vector2d& offset) override {}
#endif
  void DragCancel() override;
  bool IsDragDropInProgress() override;
  void AddObserver(aura::client::DragDropClientObserver* observer) override {}
  void RemoveObserver(aura::client::DragDropClientObserver* observer) override {
  }

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;

 private:
  // True while receiving ui::LocatedEvents for drag operations.
  bool drag_in_progress_ = false;

  // Target window where drag operations are occurring.
  raw_ptr<aura::Window> target_ = nullptr;
};

TestDragDropClient::TestDragDropClient() = default;

TestDragDropClient::~TestDragDropClient() = default;

DragOperation TestDragDropClient::StartDragAndDrop(
    std::unique_ptr<ui::OSExchangeData> data,
    aura::Window* root_window,
    aura::Window* source_window,
    const gfx::Point& screen_location,
    int allowed_operations,
    ui::mojom::DragEventSource source) {
  if (IsDragDropInProgress())
    return DragOperation::kNone;
  drag_in_progress_ = true;
  target_ = root_window;
  return ui::PreferredDragOperation(allowed_operations);
}

void TestDragDropClient::DragCancel() {
  drag_in_progress_ = false;
}

bool TestDragDropClient::IsDragDropInProgress() {
  return drag_in_progress_;
}

void TestDragDropClient::OnMouseEvent(ui::MouseEvent* event) {
  if (!IsDragDropInProgress())
    return;
  switch (event->type()) {
    case ui::EventType::kMouseDragged:
      event->StopPropagation();
      break;
    case ui::EventType::kMouseReleased:
      drag_in_progress_ = false;
      event->StopPropagation();
      break;
    default:
      break;
  }
}
#endif  // defined(USE_AURA)

// Tests if the callback is called correctly when a mouse click happens on a
// MenuButton.
TEST_F(MenuButtonTest, ActivateDropDownOnMouseClick) {
  ConfigureMenuButton(std::make_unique<TestMenuButton>());

  generator()->MoveMouseTo(button()->GetBoundsInScreen().CenterPoint());
  generator()->ClickLeftButton();

  EXPECT_TRUE(button()->clicked());
  EXPECT_EQ(Button::STATE_HOVERED, button()->last_state());
}

TEST_F(MenuButtonTest, ActivateOnKeyPress) {
  ConfigureMenuButton(std::make_unique<TestMenuButton>());

  EXPECT_FALSE(button()->clicked());
  button()->OnKeyPressed(ui::KeyEvent(ui::EventType::kKeyPressed,
                                      ui::KeyboardCode::VKEY_SPACE,
                                      ui::DomCode::SPACE, 0));
  EXPECT_TRUE(button()->clicked());

  button()->Reset();
  EXPECT_FALSE(button()->clicked());

  button()->OnKeyPressed(ui::KeyEvent(ui::EventType::kKeyPressed,
                                      ui::KeyboardCode::VKEY_RETURN,
                                      ui::DomCode::ENTER, 0));
  EXPECT_EQ(PlatformStyle::kReturnClicksFocusedControl, button()->clicked());
}

// Tests that the ink drop center point is set from the mouse click point.
TEST_F(MenuButtonTest, InkDropCenterSetFromClick) {
  ConfigureMenuButton(std::make_unique<TestMenuButton>());

  const gfx::Point click_point = button()->GetBoundsInScreen().CenterPoint();
  generator()->MoveMouseTo(click_point);
  generator()->ClickLeftButton();

  EXPECT_TRUE(button()->clicked());
  gfx::Point inkdrop_center_point =
      InkDrop::Get(button())->GetInkDropCenterBasedOnLastEvent();
  View::ConvertPointToScreen(button(), &inkdrop_center_point);
  EXPECT_EQ(click_point, inkdrop_center_point);
}

// Tests that the ink drop center point is set from the PressedLock constructor.
// TODO(crbug.com/40903656): Test flaky on MSAN ChromeOS builders.
#if BUILDFLAG(IS_CHROMEOS) && defined(MEMORY_SANITIZER)
#define MAYBE_InkDropCenterSetFromClickWithPressedLock \
  DISABLED_InkDropCenterSetFromClickWithPressedLock
#else
#define MAYBE_InkDropCenterSetFromClickWithPressedLock \
  InkDropCenterSetFromClickWithPressedLock
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) && defined(MEMORY_SANITIZER)
TEST_F(MenuButtonTest, MAYBE_InkDropCenterSetFromClickWithPressedLock) {
  ConfigureMenuButton(std::make_unique<TestMenuButton>());

  gfx::Point click_point(11, 7);
  ui::MouseEvent click_event(ui::EventType::kMousePressed, click_point,
                             click_point, base::TimeTicks(), 0, 0);
  MenuButtonController::PressedLock pressed_lock(button()->button_controller(),
                                                 false, &click_event);

  EXPECT_EQ(Button::STATE_PRESSED, button()->GetState());
  EXPECT_EQ(click_point,
            InkDrop::Get(button())->GetInkDropCenterBasedOnLastEvent());
}

// Test that the MenuButton stays pressed while there are any PressedLocks.
// TODO(crbug.com/40903656): Test flaky on MSAN ChromeOS builders.
#if BUILDFLAG(IS_CHROMEOS) && defined(MEMORY_SANITIZER)
#define MAYBE_ButtonStateForMenuButtonsWithPressedLocks \
  DISABLED_ButtonStateForMenuButtonsWithPressedLocks
#else
#define MAYBE_ButtonStateForMenuButtonsWithPressedLocks \
  ButtonStateForMenuButtonsWithPressedLocks
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) && defined(MEMORY_SANITIZER)
TEST_F(MenuButtonTest, MAYBE_ButtonStateForMenuButtonsWithPressedLocks) {
  ConfigureMenuButton(std::make_unique<TestMenuButton>());
  const gfx::Rect button_bounds = button()->GetBoundsInScreen();

  // Move the mouse over the button; the button should be in a hovered state.
  generator()->MoveMouseTo(button_bounds.CenterPoint());
  EXPECT_EQ(Button::STATE_HOVERED, button()->GetState());

  // Introduce a PressedLock, which should make the button pressed.
  auto pressed_lock1 = std::make_unique<MenuButtonController::PressedLock>(
      button()->button_controller());
  EXPECT_EQ(Button::STATE_PRESSED, button()->GetState());

  // Even if we move the mouse outside of the button, it should remain pressed.
  generator()->MoveMouseTo(button_bounds.bottom_right() + gfx::Vector2d(1, 1));
  EXPECT_EQ(Button::STATE_PRESSED, button()->GetState());

  // Creating a new lock should obviously keep the button pressed.
  auto pressed_lock2 = std::make_unique<MenuButtonController::PressedLock>(
      button()->button_controller());
  EXPECT_EQ(Button::STATE_PRESSED, button()->GetState());

  // The button should remain pressed while any locks are active.
  pressed_lock1.reset();
  EXPECT_EQ(Button::STATE_PRESSED, button()->GetState());

  // Resetting the final lock should return the button's state to normal...
  pressed_lock2.reset();
  EXPECT_EQ(Button::STATE_NORMAL, button()->GetState());

  // ...And it should respond to mouse movement again.
  generator()->MoveMouseTo(button_bounds.CenterPoint());
  EXPECT_EQ(Button::STATE_HOVERED, button()->GetState());

  // Test that the button returns to the appropriate state after the press; if
  // the mouse ends over the button, the button should be hovered.
  pressed_lock1 = button()->button_controller()->TakeLock();
  EXPECT_EQ(Button::STATE_PRESSED, button()->GetState());
  pressed_lock1.reset();
  EXPECT_EQ(Button::STATE_HOVERED, button()->GetState());

  // If the button is disabled before the pressed lock, it should be disabled
  // after the pressed lock.
  button()->SetState(Button::STATE_DISABLED);
  pressed_lock1 = button()->button_controller()->TakeLock();
  EXPECT_EQ(Button::STATE_PRESSED, button()->GetState());
  pressed_lock1.reset();
  EXPECT_EQ(Button::STATE_DISABLED, button()->GetState());

  generator()->MoveMouseTo(button_bounds.bottom_right() + gfx::Vector2d(1, 1));

  // Edge case: the button is disabled, a pressed lock is added, and then the
  // button is re-enabled. It should be enabled after the lock is removed.
  pressed_lock1 = button()->button_controller()->TakeLock();
  EXPECT_EQ(Button::STATE_PRESSED, button()->GetState());
  button()->SetState(Button::STATE_NORMAL);
  pressed_lock1.reset();
  EXPECT_EQ(Button::STATE_NORMAL, button()->GetState());
}

// Test that the MenuButton does not become pressed if it can be dragged, until
// a release occurs.
// TODO(crbug.com/40903656): Test flaky on MSAN ChromeOS builders.
#if BUILDFLAG(IS_CHROMEOS) && defined(MEMORY_SANITIZER)
#define MAYBE_DraggableMenuButtonActivatesOnRelease \
  DISABLED_DraggableMenuButtonActivatesOnRelease
#else
#define MAYBE_DraggableMenuButtonActivatesOnRelease \
  DraggableMenuButtonActivatesOnRelease
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) && defined(MEMORY_SANITIZER)
TEST_F(MenuButtonTest, MAYBE_DraggableMenuButtonActivatesOnRelease) {
  ConfigureMenuButton(std::make_unique<TestMenuButton>());
  TestDragController drag_controller;
  button()->set_drag_controller(&drag_controller);

  generator()->MoveMouseTo(button()->GetBoundsInScreen().CenterPoint());
  generator()->PressLeftButton();
  EXPECT_FALSE(button()->clicked());

  generator()->ReleaseLeftButton();
  EXPECT_TRUE(button()->clicked());
  EXPECT_EQ(Button::STATE_HOVERED, button()->last_state());
}

// TODO(crbug.com/40903656): Test flaky on MSAN ChromeOS builders.
#if BUILDFLAG(IS_CHROMEOS) && defined(MEMORY_SANITIZER)
#define MAYBE_InkDropStateForMenuButtonActivationsWithoutCallback \
  DISABLED_InkDropStateForMenuButtonActivationsWithoutCallback
#else
#define MAYBE_InkDropStateForMenuButtonActivationsWithoutCallback \
  InkDropStateForMenuButtonActivationsWithoutCallback
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) && defined(MEMORY_SANITIZER)
TEST_F(MenuButtonTest,
       MAYBE_InkDropStateForMenuButtonActivationsWithoutCallback) {
  ConfigureMenuButton(
      std::make_unique<TestMenuButton>(Button::PressedCallback()));
  ink_drop()->AnimateToState(InkDropState::ACTION_PENDING);
  button()->Activate(nullptr);

  EXPECT_EQ(InkDropState::HIDDEN, ink_drop()->GetTargetInkDropState());
}

// TODO(crbug.com/40903656): Test flaky on MSAN ChromeOS builders.
#if BUILDFLAG(IS_CHROMEOS) && defined(MEMORY_SANITIZER)
#define MAYBE_InkDropStateForMenuButtonActivationsWithCallbackThatDoesntAcquireALock \
  DISABLED_InkDropStateForMenuButtonActivationsWithCallbackThatDoesntAcquireALock
#else
#define MAYBE_InkDropStateForMenuButtonActivationsWithCallbackThatDoesntAcquireALock \
  InkDropStateForMenuButtonActivationsWithCallbackThatDoesntAcquireALock
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) && defined(MEMORY_SANITIZER)
TEST_F(
    MenuButtonTest,
    MAYBE_InkDropStateForMenuButtonActivationsWithCallbackThatDoesntAcquireALock) {
  ConfigureMenuButton(std::make_unique<TestMenuButton>());
  button()->Activate(nullptr);

  EXPECT_EQ(InkDropState::ACTION_TRIGGERED,
            ink_drop()->GetTargetInkDropState());
}

TEST_F(
    MenuButtonTest,
    InkDropStateForMenuButtonActivationsWithCallbackThatDoesntReleaseAllLocks) {
  ConfigureMenuButton(std::make_unique<PressStateButton>(false));
  button()->Activate(nullptr);

  EXPECT_EQ(InkDropState::ACTIVATED, ink_drop()->GetTargetInkDropState());
}

TEST_F(MenuButtonTest,
       InkDropStateForMenuButtonActivationsWithCallbackThatReleasesAllLocks) {
  ConfigureMenuButton(std::make_unique<PressStateButton>(true));
  button()->Activate(nullptr);

  EXPECT_EQ(InkDropState::DEACTIVATED, ink_drop()->GetTargetInkDropState());
}

// TODO(crbug.com/40903656): Test flaky on MSAN ChromeOS builders.
#if BUILDFLAG(IS_CHROMEOS) && defined(MEMORY_SANITIZER)
#define MAYBE_InkDropStateForMenuButtonsWithPressedLocks \
  DISABLED_InkDropStateForMenuButtonsWithPressedLocks
#else
#define MAYBE_InkDropStateForMenuButtonsWithPressedLocks \
  InkDropStateForMenuButtonsWithPressedLocks
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) && defined(MEMORY_SANITIZER)
TEST_F(MenuButtonTest, MAYBE_InkDropStateForMenuButtonsWithPressedLocks) {
  ConfigureMenuButton(std::make_unique<TestMenuButton>());

  auto pressed_lock1 = std::make_unique<MenuButtonController::PressedLock>(
      button()->button_controller());

  EXPECT_EQ(InkDropState::ACTIVATED, ink_drop()->GetTargetInkDropState());

  auto pressed_lock2 = std::make_unique<MenuButtonController::PressedLock>(
      button()->button_controller());

  EXPECT_EQ(InkDropState::ACTIVATED, ink_drop()->GetTargetInkDropState());

  pressed_lock1.reset();
  EXPECT_EQ(InkDropState::ACTIVATED, ink_drop()->GetTargetInkDropState());

  pressed_lock2.reset();
  EXPECT_EQ(InkDropState::DEACTIVATED, ink_drop()->GetTargetInkDropState());
}

// Verifies only one ink drop animation is triggered when multiple PressedLocks
// are attached to a MenuButton.
#if BUILDFLAG(IS_CHROMEOS) && defined(MEMORY_SANITIZER)
#define MAYBE_OneInkDropAnimationForReentrantPressedLocks \
  DISABLED_OneInkDropAnimationForReentrantPressedLocks
#else
#define MAYBE_OneInkDropAnimationForReentrantPressedLocks \
  OneInkDropAnimationForReentrantPressedLocks
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) && defined(MEMORY_SANITIZER)
TEST_F(MenuButtonTest, MAYBE_OneInkDropAnimationForReentrantPressedLocks) {
  ConfigureMenuButton(std::make_unique<TestMenuButton>());

  auto pressed_lock1 = std::make_unique<MenuButtonController::PressedLock>(
      button()->button_controller());

  EXPECT_EQ(InkDropState::ACTIVATED, ink_drop()->GetTargetInkDropState());
  ink_drop()->AnimateToState(InkDropState::ACTION_PENDING);

  auto pressed_lock2 = std::make_unique<MenuButtonController::PressedLock>(
      button()->button_controller());

  EXPECT_EQ(InkDropState::ACTION_PENDING, ink_drop()->GetTargetInkDropState());
}

// Verifies the InkDropState is left as ACTIVATED if a PressedLock is active
// before another Activation occurs.
// TODO(crbug.com/40903656): Test flaky on MSAN ChromeOS builders.
#if BUILDFLAG(IS_CHROMEOS) && defined(MEMORY_SANITIZER)
#define MAYBE_InkDropStateForMenuButtonWithPressedLockBeforeActivation \
  DISABLED_InkDropStateForMenuButtonWithPressedLockBeforeActivation
#else
#define MAYBE_InkDropStateForMenuButtonWithPressedLockBeforeActivation \
  InkDropStateForMenuButtonWithPressedLockBeforeActivation
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) && defined(MEMORY_SANITIZER)
TEST_F(MenuButtonTest,
       MAYBE_InkDropStateForMenuButtonWithPressedLockBeforeActivation) {
  ConfigureMenuButton(std::make_unique<TestMenuButton>());
  MenuButtonController::PressedLock lock(button()->button_controller());

  button()->Activate(nullptr);

  EXPECT_EQ(InkDropState::ACTIVATED, ink_drop()->GetTargetInkDropState());
}

#if defined(USE_AURA)

// Tests that the MenuButton does not become pressed if it can be dragged, and a
// DragDropClient is processing the events.
// TODO(crbug.com/40903656): Test flaky on MSAN ChromeOS builders.
#if BUILDFLAG(IS_CHROMEOS) && defined(MEMORY_SANITIZER)
#define MAYBE_DraggableMenuButtonDoesNotActivateOnDrag \
  DISABLED_DraggableMenuButtonDoesNotActivateOnDrag
#else
#define MAYBE_DraggableMenuButtonDoesNotActivateOnDrag \
  DraggableMenuButtonDoesNotActivateOnDrag
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) && defined(MEMORY_SANITIZER)
TEST_F(MenuButtonTest, MAYBE_DraggableMenuButtonDoesNotActivateOnDrag) {
  ConfigureMenuButton(std::make_unique<TestMenuButton>());
  TestDragController drag_controller;
  button()->set_drag_controller(&drag_controller);

  TestDragDropClient drag_client;
  SetDragDropClient(GetContext(), &drag_client);
  button()->AddPreTargetHandler(&drag_client,
                                ui::EventTarget::Priority::kSystem);

  generator()->DragMouseBy(10, 0);
  EXPECT_FALSE(button()->clicked());
  EXPECT_EQ(Button::STATE_NORMAL, button()->last_state());
  button()->RemovePreTargetHandler(&drag_client);
}

#endif  // USE_AURA

// No touch on desktop Mac. Tracked in http://crbug.com/445520.
#if !BUILDFLAG(IS_MAC) || defined(USE_AURA)

// Tests if the callback is notified correctly when a gesture tap happens on a
// MenuButton that has a callback.
TEST_F(MenuButtonTest, ActivateDropDownOnGestureTap) {
  ConfigureMenuButton(std::make_unique<TestMenuButton>());

  // Move the mouse outside the menu button so that it doesn't impact the
  // button state.
  generator()->MoveMouseTo(400, 400);
  EXPECT_FALSE(button()->IsMouseHovered());

  generator()->GestureTapAt(gfx::Point(10, 10));

  // Check that MenuButton has notified the callback, while it was in pressed
  // state.
  EXPECT_TRUE(button()->clicked());
  EXPECT_EQ(Button::STATE_HOVERED, button()->last_state());

  // The button should go back to its normal state since the gesture ended.
  EXPECT_EQ(Button::STATE_NORMAL, button()->GetState());
}

// Tests that the button enters a hovered state upon a tap down, before becoming
// pressed at activation.
TEST_F(MenuButtonTest, TouchFeedbackDuringTap) {
  ConfigureMenuButton(std::make_unique<TestMenuButton>());
  generator()->PressTouch();
  EXPECT_EQ(Button::STATE_HOVERED, button()->GetState());

  generator()->ReleaseTouch();
  EXPECT_EQ(Button::STATE_HOVERED, button()->last_state());
}

// Tests that a move event that exits the button returns it to the normal state,
// and that the button did not activate the callback.
TEST_F(MenuButtonTest, TouchFeedbackDuringTapCancel) {
  ConfigureMenuButton(std::make_unique<TestMenuButton>());
  generator()->PressTouch();
  EXPECT_EQ(Button::STATE_HOVERED, button()->GetState());

  generator()->MoveTouch(gfx::Point(10, 30));
  generator()->ReleaseTouch();
  EXPECT_EQ(Button::STATE_NORMAL, button()->GetState());
  EXPECT_FALSE(button()->clicked());
}

#endif  // !BUILDFLAG(IS_MAC) || defined(USE_AURA)

TEST_F(MenuButtonTest, InkDropHoverWhenShowingMenu) {
  ConfigureMenuButton(std::make_unique<PressStateButton>(false));

  generator()->MoveMouseTo(GetOutOfButtonLocation());
  EXPECT_FALSE(ink_drop()->is_hovered());

  generator()->MoveMouseTo(button()->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(ink_drop()->is_hovered());

  generator()->PressLeftButton();
  EXPECT_FALSE(ink_drop()->is_hovered());
}

TEST_F(MenuButtonTest, InkDropIsHoveredAfterDismissingMenuWhenMouseOverButton) {
  auto press_state_button = std::make_unique<PressStateButton>(false);
  auto* test_button = press_state_button.get();
  ConfigureMenuButton(std::move(press_state_button));

  generator()->MoveMouseTo(button()->GetBoundsInScreen().CenterPoint());
  generator()->PressLeftButton();
  EXPECT_FALSE(ink_drop()->is_hovered());
  test_button->ReleasePressedLock();

  EXPECT_TRUE(ink_drop()->is_hovered());
}

TEST_F(MenuButtonTest,
       InkDropIsntHoveredAfterDismissingMenuWhenMouseOutsideButton) {
  auto press_state_button = std::make_unique<PressStateButton>(false);
  auto* test_button = press_state_button.get();
  ConfigureMenuButton(std::move(press_state_button));

  generator()->MoveMouseTo(button()->GetBoundsInScreen().CenterPoint());
  generator()->PressLeftButton();
  generator()->MoveMouseTo(GetOutOfButtonLocation());
  test_button->ReleasePressedLock();

  EXPECT_FALSE(ink_drop()->is_hovered());
}

// This test ensures there isn't a UAF in MenuButton::OnGestureEvent() if the
// button callback deletes the MenuButton.
TEST_F(MenuButtonTest, DestroyButtonInGesture) {
  std::unique_ptr<TestMenuButton> test_menu_button =
      std::make_unique<TestMenuButton>(base::BindRepeating(
          [](std::unique_ptr<TestMenuButton>* button) { button->reset(); },
          &test_menu_button));
  ConfigureMenuButton(std::move(test_menu_button));

  ui::GestureEvent gesture_event(
      0, 0, 0, base::TimeTicks::Now(),
      ui::GestureEventDetails(ui::EventType::kGestureTap));
  button()->OnGestureEvent(&gesture_event);
}

TEST_F(MenuButtonTest, AccessibleDefaultActionVerb) {
  ConfigureMenuButton(std::make_unique<TestMenuButton>());
  ui::AXNodeData data;

  button()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetDefaultActionVerb(), ax::mojom::DefaultActionVerb::kOpen);

  data = ui::AXNodeData();
  button()->SetEnabled(false);
  button()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_FALSE(
      data.HasIntAttribute(ax::mojom::IntAttribute::kDefaultActionVerb));

  data = ui::AXNodeData();
  button()->SetEnabled(true);
  button()->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.GetDefaultActionVerb(), ax::mojom::DefaultActionVerb::kOpen);
}

}  // namespace views
