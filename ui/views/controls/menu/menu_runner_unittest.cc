// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_runner.h"

#include <stdint.h>

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "build/build_config.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/compositor.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner_impl.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/controls/menu/test_menu_item_view.h"
#include "ui/views/test/menu_test_utils.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_utils.h"

#if BUILDFLAG(IS_MAC)
#include "ui/views/controls/menu/menu_cocoa_watcher_mac.h"
#endif

namespace views::test {

class MenuRunnerTest : public ViewsTestBase {
 public:
  MenuRunnerTest() = default;

  MenuRunnerTest(const MenuRunnerTest&) = delete;
  MenuRunnerTest& operator=(const MenuRunnerTest&) = delete;

  ~MenuRunnerTest() override = default;

  // Creates a `TestMenuItemView` and retains a raw pointer to it. Call
  // `ResetMenuItemView` if you destroy it prior to test tear-down.
  std::unique_ptr<MenuItemView> CreateMenuItemView() {
    auto menu_item_view =
        std::make_unique<TestMenuItemView>(menu_delegate_.get());
    menu_item_view->AppendMenuItem(1, u"One");
    menu_item_view->AppendMenuItem(2, u"\x062f\x0648");
    menu_item_view_ = menu_item_view.get();
    return menu_item_view;
  }

  void ResetMenuItemView() { menu_item_view_ = nullptr; }

  // Creates a menuRunner with `run_types`.
  void InitMenuRunner(int32_t run_types) {
    menu_runner_ =
        std::make_unique<MenuRunner>(CreateMenuItemView(), run_types);
  }

  views::TestMenuItemView* menu_item_view() { return menu_item_view_; }
  TestMenuDelegate* menu_delegate() { return menu_delegate_.get(); }
  MenuRunner* menu_runner() { return menu_runner_.get(); }
  Widget* owner() { return owner_.get(); }

  void SetUp() override {
    ViewsTestBase::SetUp();

#if BUILDFLAG(IS_MAC)
    // Ignore app activation notifications during tests (they make the tests
    // flaky).
    MenuCocoaWatcherMac::SetNotificationFilterForTesting(
        MacNotificationFilter::IgnoreWorkspaceNotifications);
#endif

    menu_delegate_ = std::make_unique<TestMenuDelegate>();
    Widget::InitParams params = CreateParams(
        Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_POPUP);
    owner_ = std::make_unique<Widget>();
    owner_->Init(std::move(params));
    owner_->Show();
  }

  // ViewsTestBase:
  void TearDown() override {
    ResetMenuItemView();
    if (owner_)
      owner_->CloseNow();

#if BUILDFLAG(IS_MAC)
    MenuCocoaWatcherMac::SetNotificationFilterForTesting(
        MacNotificationFilter::DontIgnoreNotifications);
#endif

    ViewsTestBase::TearDown();
  }

  bool IsItemSelected(int command_id) {
    MenuItemView* item = menu_item_view()->GetMenuItemByID(command_id);
    return item ? item->IsSelected() : false;
  }

  // Menus that use prefix selection don't support mnemonics - the input is
  // always part of the prefix.
  bool MenuSupportsMnemonics() {
    return !MenuConfig::instance().all_menus_use_prefix_selection;
  }

 private:
  // Owned by menu_runner_.
  raw_ptr<views::TestMenuItemView> menu_item_view_ = nullptr;

  std::unique_ptr<TestMenuDelegate> menu_delegate_;
  std::unique_ptr<MenuRunner> menu_runner_;
  std::unique_ptr<Widget> owner_;
};

// Tests that MenuRunner is still running after the call to RunMenuAt when
// initialized with , and that MenuDelegate is notified upon
// the closing of the menu.
TEST_F(MenuRunnerTest, AsynchronousRun) {
  InitMenuRunner(0);
  MenuRunner* runner = menu_runner();
  runner->RunMenuAt(owner(), nullptr, gfx::Rect(), MenuAnchorPosition::kTopLeft,
                    ui::MENU_SOURCE_NONE, nullptr);
  EXPECT_TRUE(runner->IsRunning());

  runner->Cancel();
  EXPECT_FALSE(runner->IsRunning());
  TestMenuDelegate* delegate = menu_delegate();
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, delegate->on_menu_closed_menu());
}

// Tests that when a menu is run asynchronously, key events are handled properly
// by testing that Escape key closes the menu.
TEST_F(MenuRunnerTest, AsynchronousKeyEventHandling) {
  InitMenuRunner(0);
  MenuRunner* runner = menu_runner();
  runner->RunMenuAt(owner(), nullptr, gfx::Rect(), MenuAnchorPosition::kTopLeft,
                    ui::MENU_SOURCE_NONE, nullptr);
  EXPECT_TRUE(runner->IsRunning());

  ui::test::EventGenerator generator(GetContext(), owner()->GetNativeWindow());
  generator.PressKey(ui::VKEY_ESCAPE, 0);
  EXPECT_FALSE(runner->IsRunning());
  TestMenuDelegate* delegate = menu_delegate();
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_EQ(nullptr, delegate->on_menu_closed_menu());
}

// Tests that a key press on a US keyboard layout activates the correct menu
// item.
// This test is flaky on ozone (https://crbug.com/1197217).
#if BUILDFLAG(IS_OZONE)
#define MAYBE_LatinMnemonic DISABLED_LatinMnemonic
#else
#define MAYBE_LatinMnemonic LatinMnemonic
#endif
TEST_F(MenuRunnerTest, MAYBE_LatinMnemonic) {
  if (!MenuSupportsMnemonics())
    return;

  views::test::DisableMenuClosureAnimations();
  InitMenuRunner(0);
  MenuRunner* runner = menu_runner();
  runner->RunMenuAt(owner(), nullptr, gfx::Rect(), MenuAnchorPosition::kTopLeft,
                    ui::MENU_SOURCE_NONE, nullptr);
  EXPECT_TRUE(runner->IsRunning());

  ui::test::EventGenerator generator(GetContext(), owner()->GetNativeWindow());
  generator.PressKey(ui::VKEY_O, 0);
  views::test::WaitForMenuClosureAnimation();
  EXPECT_FALSE(runner->IsRunning());
  TestMenuDelegate* delegate = menu_delegate();
  EXPECT_EQ(1, delegate->execute_command_id());
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_NE(nullptr, delegate->on_menu_closed_menu());
}

#if !BUILDFLAG(IS_WIN)
// Tests that a key press on a non-US keyboard layout activates the correct menu
// item. Disabled on Windows because a WM_CHAR event does not activate an item.
TEST_F(MenuRunnerTest, NonLatinMnemonic) {
  if (!MenuSupportsMnemonics())
    return;

  views::test::DisableMenuClosureAnimations();
  InitMenuRunner(0);
  MenuRunner* runner = menu_runner();
  runner->RunMenuAt(owner(), nullptr, gfx::Rect(), MenuAnchorPosition::kTopLeft,
                    ui::MENU_SOURCE_NONE, nullptr);
  EXPECT_TRUE(runner->IsRunning());

  ui::test::EventGenerator generator(GetContext(), owner()->GetNativeWindow());
  ui::KeyEvent key_press =
      ui::KeyEvent::FromCharacter(0x062f, ui::VKEY_N, ui::DomCode::NONE, 0);
  generator.Dispatch(&key_press);
  views::test::WaitForMenuClosureAnimation();
  EXPECT_FALSE(runner->IsRunning());
  TestMenuDelegate* delegate = menu_delegate();
  EXPECT_EQ(2, delegate->execute_command_id());
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_NE(nullptr, delegate->on_menu_closed_menu());
}
#endif  // !BUILDFLAG(IS_WIN)

TEST_F(MenuRunnerTest, MenuItemViewShowsMnemonics) {
  if (!MenuSupportsMnemonics())
    return;

  InitMenuRunner(MenuRunner::HAS_MNEMONICS | MenuRunner::SHOULD_SHOW_MNEMONICS);

  menu_runner()->RunMenuAt(owner(), nullptr, gfx::Rect(),
                           MenuAnchorPosition::kTopLeft, ui::MENU_SOURCE_NONE,
                           nullptr);

  EXPECT_TRUE(menu_item_view()->show_mnemonics());
}

TEST_F(MenuRunnerTest, MenuItemViewDoesNotShowMnemonics) {
  if (!MenuSupportsMnemonics())
    return;

  InitMenuRunner(MenuRunner::HAS_MNEMONICS);

  menu_runner()->RunMenuAt(owner(), nullptr, gfx::Rect(),
                           MenuAnchorPosition::kTopLeft, ui::MENU_SOURCE_NONE,
                           nullptr);

  EXPECT_FALSE(menu_item_view()->show_mnemonics());
}

TEST_F(MenuRunnerTest, PrefixSelect) {
  if (!MenuConfig::instance().all_menus_use_prefix_selection)
    return;

  base::SimpleTestTickClock clock;

  // This test has a menu with three items:
  //   { 1, "One" }
  //   { 2, "\x062f\x0648" }
  //   { 3, "One Two" }
  // It progressively prefix searches for "One " (note the space) and ensures
  // that the right item is found.

  views::test::DisableMenuClosureAnimations();
  InitMenuRunner(0);
  menu_item_view()->AppendMenuItem(3, u"One Two");

  MenuRunner* runner = menu_runner();
  runner->RunMenuAt(owner(), nullptr, gfx::Rect(), MenuAnchorPosition::kTopLeft,
                    ui::MENU_SOURCE_NONE, nullptr);
  EXPECT_TRUE(runner->IsRunning());

  menu_item_view()
      ->GetSubmenu()
      ->GetPrefixSelector()
      ->set_tick_clock_for_testing(&clock);

  ui::test::EventGenerator generator(GetContext(), owner()->GetNativeWindow());
  generator.PressKey(ui::VKEY_O, 0);
  EXPECT_TRUE(IsItemSelected(1));
  generator.PressKey(ui::VKEY_N, 0);
  generator.PressKey(ui::VKEY_E, 0);
  EXPECT_TRUE(IsItemSelected(1));

  generator.PressKey(ui::VKEY_SPACE, 0);
  EXPECT_TRUE(IsItemSelected(3));

  // Wait out the PrefixSelector's timeout.
  clock.Advance(base::Seconds(10));

  // Send Space to activate the selected menu item.
  generator.PressKey(ui::VKEY_SPACE, 0);
  views::test::WaitForMenuClosureAnimation();
  EXPECT_FALSE(runner->IsRunning());
  TestMenuDelegate* delegate = menu_delegate();
  EXPECT_EQ(3, delegate->execute_command_id());
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_NE(nullptr, delegate->on_menu_closed_menu());
}

// This test is Mac-specific: Mac is the only platform where VKEY_SPACE
// activates menu items.
#if BUILDFLAG(IS_MAC)
TEST_F(MenuRunnerTest, SpaceActivatesItem) {
  if (!MenuConfig::instance().all_menus_use_prefix_selection)
    return;

  views::test::DisableMenuClosureAnimations();
  InitMenuRunner(0);

  MenuRunner* runner = menu_runner();
  runner->RunMenuAt(owner(), nullptr, gfx::Rect(), MenuAnchorPosition::kTopLeft,
                    ui::MENU_SOURCE_NONE, nullptr);
  EXPECT_TRUE(runner->IsRunning());

  ui::test::EventGenerator generator(GetContext(), owner()->GetNativeWindow());
  generator.PressKey(ui::VKEY_DOWN, 0);
  EXPECT_TRUE(IsItemSelected(1));
  generator.PressKey(ui::VKEY_SPACE, 0);
  views::test::WaitForMenuClosureAnimation();

  EXPECT_FALSE(runner->IsRunning());
  TestMenuDelegate* delegate = menu_delegate();
  EXPECT_EQ(1, delegate->execute_command_id());
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_NE(nullptr, delegate->on_menu_closed_menu());
}
#endif  // BUILDFLAG(IS_MAC)

// Tests that attempting to nest a menu within a drag-and-drop menu does not
// cause a crash. Instead the drag and drop action should be canceled, and the
// new menu should be openned.
TEST_F(MenuRunnerTest, NestingDuringDrag) {
  InitMenuRunner(MenuRunner::FOR_DROP);
  MenuRunner* runner = menu_runner();
  runner->RunMenuAt(owner(), nullptr, gfx::Rect(), MenuAnchorPosition::kTopLeft,
                    ui::MENU_SOURCE_NONE, nullptr);
  EXPECT_TRUE(runner->IsRunning());

  auto nested_delegate = std::make_unique<TestMenuDelegate>();
  MenuRunner nested_runner(
      MenuRunner(std::make_unique<MenuItemView>(nested_delegate.get()),
                 MenuRunner::IS_NESTED));
  nested_runner.RunMenuAt(owner(), nullptr, gfx::Rect(),
                          MenuAnchorPosition::kTopLeft, ui::MENU_SOURCE_NONE,
                          nullptr);
  EXPECT_TRUE(nested_runner.IsRunning());
  EXPECT_FALSE(runner->IsRunning());
  EXPECT_EQ(1, menu_delegate()->on_menu_closed_called());
  EXPECT_NE(nullptr, menu_delegate()->on_menu_closed_menu());
}

namespace {

// An EventHandler that launches a menu in response to a mouse press.
class MenuLauncherEventHandler : public ui::EventHandler {
 public:
  MenuLauncherEventHandler(MenuRunner* runner, Widget* owner)
      : runner_(runner), owner_(owner) {}

  MenuLauncherEventHandler(const MenuLauncherEventHandler&) = delete;
  MenuLauncherEventHandler& operator=(const MenuLauncherEventHandler&) = delete;

  ~MenuLauncherEventHandler() override = default;

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::EventType::kMousePressed) {
      runner_->RunMenuAt(owner_, nullptr, gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, ui::MENU_SOURCE_NONE,
                         nullptr);
      event->SetHandled();
    }
  }

  const raw_ptr<MenuRunner> runner_;
  const raw_ptr<Widget> owner_;
};

}  // namespace

// Test harness that includes a parent Widget and View invoking the menu.
class MenuRunnerWidgetTest : public MenuRunnerTest {
 public:
  static constexpr int kEventCountViewID = 123;

  MenuRunnerWidgetTest() = default;

  MenuRunnerWidgetTest(const MenuRunnerWidgetTest&) = delete;
  MenuRunnerWidgetTest& operator=(const MenuRunnerWidgetTest&) = delete;

  Widget* widget() { return widget_.get(); }
  EventCountView* event_count_view() {
    return static_cast<EventCountView*>(
        widget()->GetRootView()->GetViewByID(kEventCountViewID));
  }

  std::unique_ptr<ui::test::EventGenerator> EventGeneratorForWidget(
      Widget* widget) {
    return std::make_unique<ui::test::EventGenerator>(
        GetContext(), widget->GetNativeWindow());
  }

  void AddMenuLauncherEventHandler(Widget* widget) {
    consumer_ =
        std::make_unique<MenuLauncherEventHandler>(menu_runner(), widget);
    event_count_view()->AddPostTargetHandler(consumer_.get());
  }

  // ViewsTestBase:
  void SetUp() override {
    MenuRunnerTest::SetUp();
    widget_ = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW);
    widget_->Init(std::move(params));
    widget_->Show();
    widget_->SetSize(gfx::Size(300, 300));

    auto event_count_view = std::make_unique<EventCountView>();
    event_count_view->SetBounds(0, 0, 300, 300);
    event_count_view->SetID(kEventCountViewID);
    widget_->GetRootView()->AddChildView(std::move(event_count_view));

    InitMenuRunner(0);
  }

  void TearDown() override {
    consumer_.reset();
    widget_->CloseNow();
    MenuRunnerTest::TearDown();
  }

 private:
  std::unique_ptr<Widget> widget_;
  std::unique_ptr<MenuLauncherEventHandler> consumer_;
};

// Tests that when a mouse press launches a menu, that the target widget does
// not take explicit capture, nor closes the menu.
TEST_F(MenuRunnerWidgetTest, WidgetDoesntTakeCapture) {
  AddMenuLauncherEventHandler(owner());

  EXPECT_EQ(gfx::NativeView(), internal::NativeWidgetPrivate::GetGlobalCapture(
                                   widget()->GetNativeView()));
  auto generator(EventGeneratorForWidget(widget()));
  generator->MoveMouseTo(widget()->GetClientAreaBoundsInScreen().CenterPoint());
  // Implicit capture should not be held by |widget|.
  generator->PressLeftButton();
  EXPECT_EQ(1, event_count_view()->GetEventCount(ui::EventType::kMousePressed));
  EXPECT_NE(widget()->GetNativeView(),
            internal::NativeWidgetPrivate::GetGlobalCapture(
                widget()->GetNativeView()));

  // The menu should still be open.
  TestMenuDelegate* delegate = menu_delegate();
  EXPECT_TRUE(menu_runner()->IsRunning());
  EXPECT_EQ(0, delegate->on_menu_closed_called());
}

// Tests that after showing a menu on mouse press, that the subsequent mouse
// will be delivered to the correct view, and not to the one that showed the
// menu.
//
// The original bug is reproducible only when showing the menu on mouse press,
// as RootView::OnMouseReleased() doesn't have the same behavior.
TEST_F(MenuRunnerWidgetTest, ClearsMouseHandlerOnRun) {
  AddMenuLauncherEventHandler(widget());

  // Create a second view that's supposed to get the second mouse press.
  EventCountView* second_event_count_view = new EventCountView();
  widget()->GetRootView()->AddChildView(second_event_count_view);

  widget()->SetBounds(gfx::Rect(0, 0, 200, 100));
  event_count_view()->SetBounds(0, 0, 100, 100);
  second_event_count_view->SetBounds(100, 0, 100, 100);

  // Click on the first view to show the menu.
  auto generator(EventGeneratorForWidget(widget()));
  generator->MoveMouseTo(event_count_view()->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();

  // Pretend we dismissed the menu using normal means, as it doesn't matter.
  EXPECT_TRUE(menu_runner()->IsRunning());
  menu_runner()->Cancel();

  // EventGenerator won't allow us to re-send the left button press without
  // releasing it first. We can't send the release event using the same
  // generator as it would be handled by the RootView in the main Widget.
  // In actual application the RootView doesn't see the release event.
  generator.reset();
  generator = EventGeneratorForWidget(widget());

  generator->MoveMouseTo(
      second_event_count_view->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  EXPECT_EQ(
      1, second_event_count_view->GetEventCount(ui::EventType::kMousePressed));
}

class MenuRunnerImplTest : public MenuRunnerTest {
 public:
  MenuRunnerImplTest() = default;

  MenuRunnerImplTest(const MenuRunnerImplTest&) = delete;
  MenuRunnerImplTest& operator=(const MenuRunnerImplTest&) = delete;

  ~MenuRunnerImplTest() override = default;
};

// Tests that when nested menu runners are destroyed out of order, that
// MenuController is not accessed after it has been destroyed. This should not
// crash on ASAN bots.
TEST_F(MenuRunnerImplTest, NestedMenuRunnersDestroyedOutOfOrder) {
  internal::MenuRunnerImpl* menu_runner =
      new internal::MenuRunnerImpl(CreateMenuItemView());
  menu_runner->RunMenuAt(owner(), nullptr, gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, 0, nullptr);

  std::unique_ptr<TestMenuDelegate> menu_delegate2(new TestMenuDelegate);
  MenuItemView* menu_item_view2 = new MenuItemView(menu_delegate2.get());
  menu_item_view2->AppendMenuItem(1, u"One");

  internal::MenuRunnerImpl* menu_runner2 = new internal::MenuRunnerImpl(
      base::WrapUnique<MenuItemView>(menu_item_view2));
  menu_runner2->RunMenuAt(owner(), nullptr, gfx::Rect(),
                          MenuAnchorPosition::kTopLeft, MenuRunner::IS_NESTED,
                          nullptr);

  // Hide the controller so we can test out of order destruction.
  MenuControllerTestApi menu_controller;
  menu_controller.SetShowing(false);

  // This destroyed MenuController
  menu_runner->OnMenuClosed(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
                            nullptr, 0);

  // This should not access the destroyed MenuController
  menu_runner2->Release();
  ResetMenuItemView();
  menu_runner->Release();
}

// Tests that when there are two separate MenuControllers, and the active one is
// deleted first, that shutting down the MenuRunner of the original
// MenuController properly closes its controller. This should not crash on ASAN
// bots.
TEST_F(MenuRunnerImplTest, MenuRunnerDestroyedWithNoActiveController) {
  internal::MenuRunnerImpl* menu_runner =
      new internal::MenuRunnerImpl(CreateMenuItemView());
  menu_runner->RunMenuAt(owner(), nullptr, gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, 0, nullptr);

  // Hide the menu, and clear its item selection state.
  MenuControllerTestApi menu_controller;
  menu_controller.SetShowing(false);
  menu_controller.ClearState();

  std::unique_ptr<TestMenuDelegate> menu_delegate2(new TestMenuDelegate);
  MenuItemView* menu_item_view2 = new MenuItemView(menu_delegate2.get());
  menu_item_view2->AppendMenuItem(1, u"One");

  internal::MenuRunnerImpl* menu_runner2 = new internal::MenuRunnerImpl(
      base::WrapUnique<MenuItemView>(menu_item_view2));
  menu_runner2->RunMenuAt(owner(), nullptr, gfx::Rect(),
                          MenuAnchorPosition::kTopLeft, MenuRunner::FOR_DROP,
                          nullptr);

  EXPECT_NE(menu_controller.controller(), MenuController::GetActiveInstance());
  menu_controller.SetShowing(true);

  // Close the runner with the active menu first.
  menu_runner2->Release();
  // Even though there is no active menu, this should still cleanup the
  // controller that it created.
  ResetMenuItemView();
  menu_runner->Release();

  // This is not expected to run, however this is from the origin ASAN stack
  // traces. So regressions will be caught with the same stack trace.
  if (menu_controller.controller())
    menu_controller.controller()->Cancel(MenuController::ExitType::kAll);
  EXPECT_EQ(nullptr, menu_controller.controller());
}

// Test class which overrides the ViewsDelegate. Allowing to simulate shutdown
// during its release.
class MenuRunnerDestructionTest : public MenuRunnerTest {
 public:
  MenuRunnerDestructionTest() = default;

  MenuRunnerDestructionTest(const MenuRunnerDestructionTest&) = delete;
  MenuRunnerDestructionTest& operator=(const MenuRunnerDestructionTest&) =
      delete;

  ~MenuRunnerDestructionTest() override = default;

  base::WeakPtr<internal::MenuRunnerImpl> MenuRunnerAsWeakPtr(
      internal::MenuRunnerImpl* menu_runner);

  // ViewsTestBase:
  void SetUp() override;
};

base::WeakPtr<internal::MenuRunnerImpl>
MenuRunnerDestructionTest::MenuRunnerAsWeakPtr(
    internal::MenuRunnerImpl* menu_runner) {
  return menu_runner->weak_factory_.GetWeakPtr();
}

void MenuRunnerDestructionTest::SetUp() {
  set_views_delegate(std::make_unique<ReleaseRefTestViewsDelegate>());
  MenuRunnerTest::SetUp();
}

// Tests that when ViewsDelegate is released that a nested Cancel of the
// MenuRunner does not occur.
TEST_F(MenuRunnerDestructionTest, MenuRunnerDestroyedDuringReleaseRef) {
  internal::MenuRunnerImpl* menu_runner =
      new internal::MenuRunnerImpl(CreateMenuItemView());
  menu_runner->RunMenuAt(owner(), nullptr, gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, 0, nullptr);

  base::RunLoop run_loop;
  static_cast<ReleaseRefTestViewsDelegate*>(test_views_delegate())
      ->set_release_ref_callback(base::BindLambdaForTesting([&]() {
        run_loop.Quit();
        ResetMenuItemView();
        menu_runner->Release();
      }));

  base::WeakPtr<internal::MenuRunnerImpl> ref(MenuRunnerAsWeakPtr(menu_runner));
  MenuControllerTestApi menu_controller;
  // This will release the ref on ViewsDelegate. The test version will release
  // |menu_runner| simulating device shutdown.
  menu_controller.controller()->Cancel(MenuController::ExitType::kAll);
  // Both the |menu_runner| and |menu_controller| should have been deleted.
  EXPECT_EQ(nullptr, menu_controller.controller());
  run_loop.Run();
  EXPECT_EQ(nullptr, ref);
}

TEST_F(MenuRunnerImplTest, FocusOnMenuClose) {
  internal::MenuRunnerImpl* menu_runner =
      new internal::MenuRunnerImpl(CreateMenuItemView());

  // Create test button that has focus.
  auto button_managed = std::make_unique<LabelButton>();
  button_managed->SetID(1);
  button_managed->SetSize(gfx::Size(20, 20));
  LabelButton* button =
      owner()->GetRootView()->AddChildView(std::move(button_managed));

  button->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  button->GetWidget()->widget_delegate()->SetCanActivate(true);
  button->GetWidget()->Activate();
  button->RequestFocus();

  // Open the menu.
  menu_runner->RunMenuAt(owner(), nullptr, gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, 0, nullptr);

  MenuControllerTestApi menu_controller;
  menu_controller.SetShowing(false);

  // Test that closing the menu sends the kFocusAfterMenuClose event.
  bool focus_after_menu_close_sent = false;
  ViewAccessibility::AccessibilityEventsCallback accessibility_events_callback =
      base::BindRepeating(
          [](bool* focus_after_menu_close_sent,
             const ui::AXPlatformNodeDelegate* delegate,
             const ax::mojom::Event event_type) {
            if (event_type == ax::mojom::Event::kFocusAfterMenuClose)
              *focus_after_menu_close_sent = true;
          },
          &focus_after_menu_close_sent);
  button->GetViewAccessibility().set_accessibility_events_callback(
      std::move(accessibility_events_callback));
  menu_runner->OnMenuClosed(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
                            nullptr, 0);

  EXPECT_TRUE(focus_after_menu_close_sent);

  // Set the callback to a no-op to avoid accessing
  // "focus_after_menu_close_sent" after this test has completed.
  button->GetViewAccessibility().set_accessibility_events_callback(
      base::DoNothing());

  ResetMenuItemView();
  menu_runner->Release();
}

TEST_F(MenuRunnerImplTest, FocusOnMenuCloseDeleteAfterRun) {
  // Create test button that has focus.
  LabelButton* button = new LabelButton(
      Button::PressedCallback(), std::u16string(), style::CONTEXT_BUTTON);
  button->SetID(1);
  button->SetSize(gfx::Size(20, 20));
  owner()->GetRootView()->AddChildView(button);
  button->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  button->GetWidget()->widget_delegate()->SetCanActivate(true);
  button->GetWidget()->Activate();
  button->RequestFocus();

  internal::MenuRunnerImpl* menu_runner =
      new internal::MenuRunnerImpl(CreateMenuItemView());
  menu_runner->RunMenuAt(owner(), nullptr, gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, 0, nullptr);

  // Hide the menu, and clear its item selection state.
  MenuControllerTestApi menu_controller;
  menu_controller.SetShowing(false);
  menu_controller.ClearState();

  std::unique_ptr<TestMenuDelegate> menu_delegate2(new TestMenuDelegate);
  MenuItemView* menu_item_view2 = new MenuItemView(menu_delegate2.get());
  menu_item_view2->AppendMenuItem(1, u"One");

  internal::MenuRunnerImpl* menu_runner2 = new internal::MenuRunnerImpl(
      base::WrapUnique<MenuItemView>(menu_item_view2));
  menu_runner2->RunMenuAt(owner(), nullptr, gfx::Rect(),
                          MenuAnchorPosition::kTopLeft, MenuRunner::FOR_DROP,
                          nullptr);

  EXPECT_NE(menu_controller.controller(), MenuController::GetActiveInstance());
  menu_controller.SetShowing(true);

  // Test that closing the menu sends the kFocusAfterMenuClose event.
  bool focus_after_menu_close_sent = false;
  ViewAccessibility::AccessibilityEventsCallback accessibility_events_callback =
      base::BindRepeating(
          [](bool* focus_after_menu_close_sent,
             const ui::AXPlatformNodeDelegate* delegate,
             const ax::mojom::Event event_type) {
            if (event_type == ax::mojom::Event::kFocusAfterMenuClose)
              *focus_after_menu_close_sent = true;
          },
          &focus_after_menu_close_sent);
  button->GetViewAccessibility().set_accessibility_events_callback(
      std::move(accessibility_events_callback));
  menu_runner2->Release();

  EXPECT_TRUE(focus_after_menu_close_sent);
  focus_after_menu_close_sent = false;
  ResetMenuItemView();
  menu_runner->Release();

  EXPECT_TRUE(focus_after_menu_close_sent);

  // Set the callback to a no-op to avoid accessing
  // "focus_after_menu_close_sent" after this test has completed.
  button->GetViewAccessibility().set_accessibility_events_callback(
      base::DoNothing());

  // This is not expected to run, however this is from the origin ASAN stack
  // traces. So regressions will be caught with the same stack trace.
  if (menu_controller.controller())
    menu_controller.controller()->Cancel(MenuController::ExitType::kAll);
  EXPECT_EQ(nullptr, menu_controller.controller());
}

// Tests that passing a histogram name to RunMenuAt records a histogram entry.
TEST_F(MenuRunnerTest, ShowMenuHostDurationMetricsDoesLog) {
  base::HistogramTester histogram_tester;
  std::string histogram_name =
      "Chrome.AppMenu.MenuHostInitToNextFramePresented";

  InitMenuRunner(0);
  MenuRunner* runner = menu_runner();
  runner->RunMenuAt(owner(), nullptr, gfx::Rect(), MenuAnchorPosition::kTopLeft,
                    ui::MENU_SOURCE_NONE, nullptr, std::nullopt,
                    histogram_name);

  base::RunLoop run_loop;
  views::MenuController::GetActiveInstance()
      ->GetSelectedMenuItem()
      ->GetSubmenu()
      ->GetWidget()
      ->GetCompositor()
      ->RequestSuccessfulPresentationTimeForNextFrame(base::BindOnce(
          [](base::RunLoop* run_loop,
             const viz::FrameTimingDetails& frame_timing_details) {
            run_loop->Quit();
          },
          &run_loop));

  histogram_tester.ExpectTotalCount(histogram_name, 0);
  run_loop.Run();
  histogram_tester.ExpectTotalCount(histogram_name, 1);
}

// Tests that not passing a histogram name to RunMenuAt does not record a
// histogram entry.
TEST_F(MenuRunnerTest, ShowMenuHostDurationMetricsDoesNotLog) {
  base::HistogramTester histogram_tester;
  std::string histogram_name =
      "Chrome.AppMenu.MenuHostInitToNextFramePresented";

  InitMenuRunner(0);
  MenuRunner* runner = menu_runner();
  runner->RunMenuAt(owner(), nullptr, gfx::Rect(), MenuAnchorPosition::kTopLeft,
                    ui::MENU_SOURCE_NONE, nullptr, std::nullopt);

  base::RunLoop run_loop;
  views::MenuController::GetActiveInstance()
      ->GetSelectedMenuItem()
      ->GetSubmenu()
      ->GetWidget()
      ->GetCompositor()
      ->RequestSuccessfulPresentationTimeForNextFrame(base::BindOnce(
          [](base::RunLoop* run_loop,
             const viz::FrameTimingDetails& frame_timing_details) {
            run_loop->Quit();
          },
          &run_loop));

  histogram_tester.ExpectTotalCount(histogram_name, 0);
  run_loop.Run();
  histogram_tester.ExpectTotalCount(histogram_name, 0);
}

}  // namespace views::test
