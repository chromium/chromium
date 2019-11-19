// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_runner.h"

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_tick_clock.h"
#include "build/build_config.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner_impl.h"
#include "ui/views/controls/menu/menu_types.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/controls/menu/test_menu_item_view.h"
#include "ui/views/test/menu_test_utils.h"
#include "ui/views/test/test_views.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace {

// Accepts a MenuRunnerImpl to release when this is. Simulates shutdown
// occurring immediately during the release of ViewsDelegate.
class DeletingTestViewsDelegate : public views::TestViewsDelegate {
 public:
  DeletingTestViewsDelegate() = default;
  ~DeletingTestViewsDelegate() override = default;

  void set_menu_runner(views::internal::MenuRunnerImpl* menu_runner) {
    menu_runner_ = menu_runner;
  }

  // views::ViewsDelegate:
  void ReleaseRef() override {
    if (menu_runner_)
      menu_runner_->Release();
  }

 private:
  // Not owned, deletes itself.
  views::internal::MenuRunnerImpl* menu_runner_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(DeletingTestViewsDelegate);
};

}  // namespace

namespace views {
namespace test {

class MenuRunnerTest : public ViewsTestBase {
 public:
  MenuRunnerTest() = default;
  ~MenuRunnerTest() override = default;

  // Initializes the delegates and views needed for a menu. It does not create
  // the MenuRunner.
  void InitMenuViews() {
    menu_delegate_ = std::make_unique<TestMenuDelegate>();
    menu_item_view_ = new views::TestMenuItemView(menu_delegate_.get());
    menu_item_view_->AppendMenuItem(1, base::ASCIIToUTF16("One"));
    menu_item_view_->AppendMenuItem(2, base::WideToUTF16(L"\x062f\x0648"));

    owner_ = std::make_unique<Widget>();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    owner_->Init(std::move(params));
    owner_->Show();
  }

  // Initializes all delegates and views needed for a menu. A MenuRunner is also
  // created with |run_types|, it takes ownership of |menu_item_view_|.
  void InitMenuRunner(int32_t run_types) {
    InitMenuViews();
    menu_runner_ = std::make_unique<MenuRunner>(menu_item_view_, run_types);
  }

  views::TestMenuItemView* menu_item_view() { return menu_item_view_; }
  TestMenuDelegate* menu_delegate() { return menu_delegate_.get(); }
  MenuRunner* menu_runner() { return menu_runner_.get(); }
  Widget* owner() { return owner_.get(); }

  // ViewsTestBase:
  void TearDown() override {
    if (owner_)
      owner_->CloseNow();
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
  views::TestMenuItemView* menu_item_view_ = nullptr;

  std::unique_ptr<TestMenuDelegate> menu_delegate_;
  std::unique_ptr<MenuRunner> menu_runner_;
  std::unique_ptr<Widget> owner_;

  DISALLOW_COPY_AND_ASSIGN(MenuRunnerTest);
};

// Tests that MenuRunner is still running after the call to RunMenuAt when
// initialized with , and that MenuDelegate is notified upon
// the closing of the menu.
TEST_F(MenuRunnerTest, AsynchronousRun) {
  InitMenuRunner(0);
  MenuRunner* runner = menu_runner();
  runner->RunMenuAt(owner(), nullptr, gfx::Rect(), MenuAnchorPosition::kTopLeft,
                    ui::MENU_SOURCE_NONE);
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
                    ui::MENU_SOURCE_NONE);
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
TEST_F(MenuRunnerTest, LatinMnemonic) {
  if (!MenuSupportsMnemonics())
    return;

  views::test::DisableMenuClosureAnimations();
  InitMenuRunner(0);
  MenuRunner* runner = menu_runner();
  runner->RunMenuAt(owner(), nullptr, gfx::Rect(), MenuAnchorPosition::kTopLeft,
                    ui::MENU_SOURCE_NONE);
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

#if !defined(OS_WIN)
// Tests that a key press on a non-US keyboard layout activates the correct menu
// item. Disabled on Windows because a WM_CHAR event does not activate an item.
TEST_F(MenuRunnerTest, NonLatinMnemonic) {
  if (!MenuSupportsMnemonics())
    return;

  views::test::DisableMenuClosureAnimations();
  InitMenuRunner(0);
  MenuRunner* runner = menu_runner();
  runner->RunMenuAt(owner(), nullptr, gfx::Rect(), MenuAnchorPosition::kTopLeft,
                    ui::MENU_SOURCE_NONE);
  EXPECT_TRUE(runner->IsRunning());

  ui::test::EventGenerator generator(GetContext(), owner()->GetNativeWindow());
  ui::KeyEvent key_press(0x062f, ui::VKEY_N, ui::DomCode::NONE, 0);
  generator.Dispatch(&key_press);
  views::test::WaitForMenuClosureAnimation();
  EXPECT_FALSE(runner->IsRunning());
  TestMenuDelegate* delegate = menu_delegate();
  EXPECT_EQ(2, delegate->execute_command_id());
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_NE(nullptr, delegate->on_menu_closed_menu());
}
#endif  // !defined(OS_WIN)

TEST_F(MenuRunnerTest, MenuItemViewShowsMnemonics) {
  if (!MenuSupportsMnemonics())
    return;

  InitMenuRunner(MenuRunner::HAS_MNEMONICS | MenuRunner::SHOULD_SHOW_MNEMONICS);

  menu_runner()->RunMenuAt(owner(), nullptr, gfx::Rect(),
                           MenuAnchorPosition::kTopLeft, ui::MENU_SOURCE_NONE);

  EXPECT_TRUE(menu_item_view()->show_mnemonics());
}

TEST_F(MenuRunnerTest, MenuItemViewDoesNotShowMnemonics) {
  if (!MenuSupportsMnemonics())
    return;

  InitMenuRunner(MenuRunner::HAS_MNEMONICS);

  menu_runner()->RunMenuAt(owner(), nullptr, gfx::Rect(),
                           MenuAnchorPosition::kTopLeft, ui::MENU_SOURCE_NONE);

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
  menu_item_view()->AppendMenuItem(3, base::ASCIIToUTF16("One Two"));

  MenuRunner* runner = menu_runner();
  runner->RunMenuAt(owner(), nullptr, gfx::Rect(), MenuAnchorPosition::kTopLeft,
                    ui::MENU_SOURCE_NONE);
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
  clock.Advance(base::TimeDelta::FromSeconds(10));

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
#if defined(OS_MACOSX)
TEST_F(MenuRunnerTest, SpaceActivatesItem) {
  if (!MenuConfig::instance().all_menus_use_prefix_selection)
    return;

  views::test::DisableMenuClosureAnimations();
  InitMenuRunner(0);

  MenuRunner* runner = menu_runner();
  runner->RunMenuAt(owner(), nullptr, gfx::Rect(), MenuAnchorPosition::kTopLeft,
                    ui::MENU_SOURCE_NONE);
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
#endif  // OS_MACOSX

// Tests that attempting to nest a menu within a drag-and-drop menu does not
// cause a crash. Instead the drag and drop action should be canceled, and the
// new menu should be openned.
TEST_F(MenuRunnerTest, NestingDuringDrag) {
  InitMenuRunner(MenuRunner::FOR_DROP);
  MenuRunner* runner = menu_runner();
  runner->RunMenuAt(owner(), nullptr, gfx::Rect(), MenuAnchorPosition::kTopLeft,
                    ui::MENU_SOURCE_NONE);
  EXPECT_TRUE(runner->IsRunning());

  std::unique_ptr<TestMenuDelegate> nested_delegate(new TestMenuDelegate);
  MenuItemView* nested_menu = new MenuItemView(nested_delegate.get());
  std::unique_ptr<MenuRunner> nested_runner(
      new MenuRunner(nested_menu, MenuRunner::IS_NESTED));
  nested_runner->RunMenuAt(owner(), nullptr, gfx::Rect(),
                           MenuAnchorPosition::kTopLeft, ui::MENU_SOURCE_NONE);
  EXPECT_TRUE(nested_runner->IsRunning());
  EXPECT_FALSE(runner->IsRunning());
  TestMenuDelegate* delegate = menu_delegate();
  EXPECT_EQ(1, delegate->on_menu_closed_called());
  EXPECT_NE(nullptr, delegate->on_menu_closed_menu());
}

namespace {

// An EventHandler that launches a menu in response to a mouse press.
class MenuLauncherEventHandler : public ui::EventHandler {
 public:
  MenuLauncherEventHandler(MenuRunner* runner, Widget* owner)
      : runner_(runner), owner_(owner) {}
  ~MenuLauncherEventHandler() override = default;

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override {
    if (event->type() == ui::ET_MOUSE_PRESSED) {
      runner_->RunMenuAt(owner_, nullptr, gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, ui::MENU_SOURCE_NONE);
      event->SetHandled();
    }
  }

  MenuRunner* runner_;
  Widget* owner_;

  DISALLOW_COPY_AND_ASSIGN(MenuLauncherEventHandler);
};

}  // namespace

// Test harness that includes a parent Widget and View invoking the menu.
class MenuRunnerWidgetTest : public MenuRunnerTest {
 public:
  MenuRunnerWidgetTest() = default;

  Widget* widget() { return widget_; }
  EventCountView* event_count_view() { return event_count_view_; }

  std::unique_ptr<ui::test::EventGenerator> EventGeneratorForWidget(
      Widget* widget) {
    return std::make_unique<ui::test::EventGenerator>(
        GetContext(), widget->GetNativeWindow());
  }

  void AddMenuLauncherEventHandler(Widget* widget) {
    consumer_ =
        std::make_unique<MenuLauncherEventHandler>(menu_runner(), widget);
    event_count_view_->AddPostTargetHandler(consumer_.get());
  }

  // ViewsTestBase:
  void SetUp() override {
    MenuRunnerTest::SetUp();
    widget_ = new Widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_WINDOW);
    widget_->Init(std::move(params));
    widget_->Show();
    widget_->SetSize(gfx::Size(300, 300));

    event_count_view_ = new EventCountView();
    event_count_view_->SetBounds(0, 0, 300, 300);
    widget_->GetRootView()->AddChildView(event_count_view_);

    InitMenuRunner(0);
  }

  void TearDown() override {
    widget_->CloseNow();
    MenuRunnerTest::TearDown();
  }

 private:
  Widget* widget_ = nullptr;
  EventCountView* event_count_view_ = nullptr;
  std::unique_ptr<MenuLauncherEventHandler> consumer_;

  DISALLOW_COPY_AND_ASSIGN(MenuRunnerWidgetTest);
};

// Tests that when a mouse press launches a menu, that the target widget does
// not take explicit capture, nor closes the menu.
TEST_F(MenuRunnerWidgetTest, WidgetDoesntTakeCapture) {
  AddMenuLauncherEventHandler(owner());

  EXPECT_EQ(gfx::kNullNativeView,
            internal::NativeWidgetPrivate::GetGlobalCapture(
                widget()->GetNativeView()));
  auto generator(EventGeneratorForWidget(widget()));
  // Implicit capture should not be held by |widget|.
  generator->PressLeftButton();
  EXPECT_EQ(1, event_count_view()->GetEventCount(ui::ET_MOUSE_PRESSED));
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
  generator->MoveMouseTo(event_count_view()->bounds().CenterPoint());
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

  generator->MoveMouseTo(second_event_count_view->bounds().CenterPoint());
  generator->PressLeftButton();
  EXPECT_EQ(1, second_event_count_view->GetEventCount(ui::ET_MOUSE_PRESSED));
}

class MenuRunnerImplTest : public MenuRunnerTest {
 public:
  MenuRunnerImplTest() = default;
  ~MenuRunnerImplTest() override = default;

  void SetUp() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MenuRunnerImplTest);
};

void MenuRunnerImplTest::SetUp() {
  MenuRunnerTest::SetUp();
  InitMenuViews();
}

// Tests that when nested menu runners are destroyed out of order, that
// MenuController is not accessed after it has been destroyed. This should not
// crash on ASAN bots.
TEST_F(MenuRunnerImplTest, NestedMenuRunnersDestroyedOutOfOrder) {
  internal::MenuRunnerImpl* menu_runner =
      new internal::MenuRunnerImpl(menu_item_view());
  menu_runner->RunMenuAt(owner(), nullptr, gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, 0);

  std::unique_ptr<TestMenuDelegate> menu_delegate2(new TestMenuDelegate);
  MenuItemView* menu_item_view2 = new MenuItemView(menu_delegate2.get());
  menu_item_view2->AppendMenuItem(1, base::ASCIIToUTF16("One"));

  internal::MenuRunnerImpl* menu_runner2 =
      new internal::MenuRunnerImpl(menu_item_view2);
  menu_runner2->RunMenuAt(owner(), nullptr, gfx::Rect(),
                          MenuAnchorPosition::kTopLeft, MenuRunner::IS_NESTED);

  // Hide the controller so we can test out of order destruction.
  MenuControllerTestApi menu_controller;
  menu_controller.SetShowing(false);

  // This destroyed MenuController
  menu_runner->OnMenuClosed(internal::MenuControllerDelegate::NOTIFY_DELEGATE,
                            nullptr, 0);

  // This should not access the destroyed MenuController
  menu_runner2->Release();
  menu_runner->Release();
}

// Tests that when there are two separate MenuControllers, and the active one is
// deleted first, that shutting down the MenuRunner of the original
// MenuController properly closes its controller. This should not crash on ASAN
// bots.
TEST_F(MenuRunnerImplTest, MenuRunnerDestroyedWithNoActiveController) {
  internal::MenuRunnerImpl* menu_runner =
      new internal::MenuRunnerImpl(menu_item_view());
  menu_runner->RunMenuAt(owner(), nullptr, gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, 0);

  // Hide the menu, and clear its item selection state.
  MenuControllerTestApi menu_controller;
  menu_controller.SetShowing(false);
  menu_controller.ClearState();

  std::unique_ptr<TestMenuDelegate> menu_delegate2(new TestMenuDelegate);
  MenuItemView* menu_item_view2 = new MenuItemView(menu_delegate2.get());
  menu_item_view2->AppendMenuItem(1, base::ASCIIToUTF16("One"));

  internal::MenuRunnerImpl* menu_runner2 =
      new internal::MenuRunnerImpl(menu_item_view2);
  menu_runner2->RunMenuAt(owner(), nullptr, gfx::Rect(),
                          MenuAnchorPosition::kTopLeft, MenuRunner::FOR_DROP);

  EXPECT_NE(menu_controller.controller(), MenuController::GetActiveInstance());
  menu_controller.SetShowing(true);

  // Close the runner with the active menu first.
  menu_runner2->Release();
  // Even though there is no active menu, this should still cleanup the
  // controller that it created.
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
  ~MenuRunnerDestructionTest() override = default;

  DeletingTestViewsDelegate* views_delegate() { return views_delegate_; }

  base::WeakPtr<internal::MenuRunnerImpl> MenuRunnerAsWeakPtr(
      internal::MenuRunnerImpl* menu_runner);

  // ViewsTestBase:
  void SetUp() override;

 private:
  // Not owned
  DeletingTestViewsDelegate* views_delegate_;

  DISALLOW_COPY_AND_ASSIGN(MenuRunnerDestructionTest);
};

base::WeakPtr<internal::MenuRunnerImpl>
MenuRunnerDestructionTest::MenuRunnerAsWeakPtr(
    internal::MenuRunnerImpl* menu_runner) {
  return menu_runner->weak_factory_.GetWeakPtr();
}

void MenuRunnerDestructionTest::SetUp() {
  std::unique_ptr<DeletingTestViewsDelegate> views_delegate(
      new DeletingTestViewsDelegate);
  views_delegate_ = views_delegate.get();
  set_views_delegate(std::move(views_delegate));
  MenuRunnerTest::SetUp();
  InitMenuViews();
}

// Tests that when ViewsDelegate is released that a nested Cancel of the
// MenuRunner does not occur.
TEST_F(MenuRunnerDestructionTest, MenuRunnerDestroyedDuringReleaseRef) {
  internal::MenuRunnerImpl* menu_runner =
      new internal::MenuRunnerImpl(menu_item_view());
  menu_runner->RunMenuAt(owner(), nullptr, gfx::Rect(),
                         MenuAnchorPosition::kTopLeft, 0);

  views_delegate()->set_menu_runner(menu_runner);

  base::WeakPtr<internal::MenuRunnerImpl> ref(MenuRunnerAsWeakPtr(menu_runner));
  MenuControllerTestApi menu_controller;
  // This will release the ref on ViewsDelegate. The test version will release
  // |menu_runner| simulating device shutdown.
  menu_controller.controller()->Cancel(MenuController::ExitType::kAll);
  // Both the |menu_runner| and |menu_controller| should have been deleted.
  EXPECT_EQ(nullptr, ref);
  EXPECT_EQ(nullptr, menu_controller.controller());
}

}  // namespace test
}  // namespace views
