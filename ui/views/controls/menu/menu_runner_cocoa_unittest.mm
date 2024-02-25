// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/controls/menu/menu_runner_impl_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#import "testing/gtest_mac.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/cocoa/menu_controller.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/events/event_utils.h"
#import "ui/events/test/cocoa_test_event_utils.h"
#include "ui/views/controls/menu/menu_cocoa_watcher_mac.h"
#include "ui/views/controls/menu/menu_runner_impl_adapter.h"
#include "ui/views/test/views_test_base.h"

namespace views::test {
namespace {

constexpr int kTestCommandId = 0;

class TestModel : public ui::SimpleMenuModel {
 public:
  TestModel() : ui::SimpleMenuModel(&delegate_), delegate_(this) {}

  TestModel(const TestModel&) = delete;
  TestModel& operator=(const TestModel&) = delete;

  void set_checked_command(int command) { checked_command_ = command; }

  void set_menu_open_callback(base::OnceClosure callback) {
    menu_open_callback_ = std::move(callback);
  }

 private:
  class Delegate : public ui::SimpleMenuModel::Delegate {
   public:
    explicit Delegate(TestModel* model) : model_(model) {}
    bool IsCommandIdChecked(int command_id) const override {
      return command_id == model_->checked_command_;
    }

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    bool IsCommandIdEnabled(int command_id) const override { return true; }
    void ExecuteCommand(int command_id, int event_flags) override {}

    void OnMenuWillShow(SimpleMenuModel* source) override {
      if (!model_->menu_open_callback_.is_null())
        std::move(model_->menu_open_callback_).Run();
    }

    bool GetAcceleratorForCommandId(
        int command_id,
        ui::Accelerator* accelerator) const override {
      if (command_id == kTestCommandId) {
        *accelerator = ui::Accelerator(ui::VKEY_E, ui::EF_CONTROL_DOWN);
        return true;
      }
      return false;
    }

   private:
    raw_ptr<TestModel> model_;
  };

 private:
  int checked_command_ = -1;
  Delegate delegate_;
  base::OnceClosure menu_open_callback_;
};

enum class MenuType { NATIVE, VIEWS };

std::string MenuTypeToString(::testing::TestParamInfo<MenuType> info) {
  return info.param == MenuType::VIEWS ? "VIEWS_MenuItemView" : "NATIVE_NSMenu";
}

}  // namespace

class MenuRunnerCocoaTest : public ViewsTestBase,
                            public ::testing::WithParamInterface<MenuType> {
 public:
  static constexpr int kWindowHeight = 200;
  static constexpr int kWindowOffset = 100;

  MenuRunnerCocoaTest() : runner_(nullptr), parent_(nullptr) {}

  MenuRunnerCocoaTest(const MenuRunnerCocoaTest&) = delete;
  MenuRunnerCocoaTest& operator=(const MenuRunnerCocoaTest&) = delete;

  ~MenuRunnerCocoaTest() override = default;

  void SetUp() override {
    const int kWindowWidth = 300;
    ViewsTestBase::SetUp();

    menu_ = std::make_unique<TestModel>();
    menu_->AddCheckItem(kTestCommandId, u"Menu Item");

    parent_ = new views::Widget();
    parent_->Init(CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS));
    parent_->SetBounds(
        gfx::Rect(kWindowOffset, kWindowOffset, kWindowWidth, kWindowHeight));
    parent_->Show();

    native_view_subview_count_ =
        [[parent_->GetNativeView().GetNativeNSView() subviews] count];

    base::RepeatingClosure on_close = base::BindRepeating(
        &MenuRunnerCocoaTest::MenuCloseCallback, base::Unretained(this));
    if (GetParam() == MenuType::NATIVE)
      runner_ = new internal::MenuRunnerImplCocoa(menu_.get(), on_close);
    else
      runner_ = new internal::MenuRunnerImplAdapter(menu_.get(), on_close);
    EXPECT_FALSE(runner_->IsRunning());
  }

  void TearDown() override {
    EXPECT_EQ(native_view_subview_count_,
              [[parent_->GetNativeView().GetNativeNSView() subviews] count]);

    if (runner_) {
      runner_.ExtractAsDangling()->Release();
    }

    // Clean up for tests that set the notification filter.
    MenuCocoaWatcherMac::SetNotificationFilterForTesting(
        MacNotificationFilter::DontIgnoreNotifications);

    parent_.ExtractAsDangling()->CloseNow();
    ViewsTestBase::TearDown();
  }

  int IsAsync() const { return GetParam() == MenuType::VIEWS; }

  // Runs the menu after registering |callback| as the menu open callback.
  void RunMenu(base::OnceClosure callback) {
    MenuCocoaWatcherMac::SetNotificationFilterForTesting(
        MacNotificationFilter::IgnoreAllNotifications);

    base::OnceClosure wrapped_callback = base::BindOnce(
        [](base::OnceClosure cb) {
          // Ignore app activation notifications while the test is running (all
          // others are OK).
          MenuCocoaWatcherMac::SetNotificationFilterForTesting(
              MacNotificationFilter::IgnoreWorkspaceNotifications);
          std::move(cb).Run();
        },
        std::move(callback));
    if (IsAsync()) {
      // Cancelling an async menu under MenuControllerCocoa::OpenMenuImpl()
      // (which invokes WillShowMenu()) will cause a UAF when that same function
      // tries to show the menu. So post a task instead.
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(wrapped_callback));
    } else {
      menu_->set_menu_open_callback(
          base::BindOnce(&MenuRunnerCocoaTest::RunMenuWrapperCallback,
                         base::Unretained(this), std::move(wrapped_callback)));
    }

    runner_->RunMenuAt(parent_, nullptr, gfx::Rect(),
                       MenuAnchorPosition::kTopLeft, MenuRunner::CONTEXT_MENU,
                       nullptr);
    MaybeRunAsync();
  }

  void MenuCancelCallback() {
    runner_->Cancel();
    if (IsAsync()) {
      // Async menus report their cancellation immediately.
      EXPECT_FALSE(runner_->IsRunning());
    } else {
      // For a synchronous menu, MenuRunner::IsRunning() should return true
      // immediately after MenuRunner::Cancel() since the menu message loop has
      // not yet terminated. It has only been marked for termination.
      EXPECT_TRUE(runner_->IsRunning());
    }
  }

  void MenuDeleteCallback() {
    runner_.ExtractAsDangling()->Release();
    // Deleting an async menu intentionally does not invoke MenuCloseCallback().
    // (The callback is typically a method on something in the process of being
    // destroyed). So invoke QuitAsyncRunLoop() here as well.
    QuitAsyncRunLoop();
  }

  NSMenu* GetNativeNSMenu() {
    if (GetParam() == MenuType::VIEWS)
      return nil;

    internal::MenuRunnerImplCocoa* cocoa_runner =
        static_cast<internal::MenuRunnerImplCocoa*>(runner_);
    return [cocoa_runner->menu_controller_ menu];
  }

  void ModelDeleteThenSelectItemCallback() {
    // AppKit may retain a reference to the NSMenu.
    NSMenu* native_menu = GetNativeNSMenu();

    // A View showing a menu typically owns a MenuRunner unique_ptr, which will
    // will be destroyed (releasing the MenuRunnerImpl) alongside the MenuModel.
    runner_.ExtractAsDangling()->Release();
    menu_ = nullptr;

    // The menu is closing (yet "alive"), but the model is destroyed. The user
    // may have already made an event to select an item in the menu. This
    // doesn't bother views menus (see MenuRunnerImpl::empty_delegate_) but
    // Cocoa menu items are refcounted and have access to a raw weak pointer in
    // the MenuController.
    if (GetParam() == MenuType::VIEWS) {
      QuitAsyncRunLoop();
      return;
    }

    EXPECT_TRUE(native_menu);

    // Simulate clicking the item using its accelerator.
    NSEvent* accelerator = cocoa_test_event_utils::KeyEventWithKeyCode(
        'e', 'e', NSEventTypeKeyDown, NSEventModifierFlagCommand);
    [native_menu performKeyEquivalent:accelerator];
  }

  void MenuCancelAndDeleteCallback() {
    runner_->Cancel();
    // Release cause the runner to delete itself.
    runner_.ExtractAsDangling()->Release();
  }

 protected:
  std::unique_ptr<TestModel> menu_;
  raw_ptr<internal::MenuRunnerImplInterface> runner_ = nullptr;
  raw_ptr<views::Widget> parent_ = nullptr;
  NSUInteger native_view_subview_count_ = 0;
  int menu_close_count_ = 0;

 private:
  void RunMenuWrapperCallback(base::OnceClosure callback) {
    EXPECT_TRUE(runner_->IsRunning());
    std::move(callback).Run();
  }

  // Run a nested run loop so that async and sync menus can be tested the
  // same way.
  void MaybeRunAsync() {
    if (!IsAsync())
      return;

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, quit_closure_, TestTimeouts::action_timeout());
    run_loop.Run();

    // |quit_closure_| should be run by QuitAsyncRunLoop(), not the timeout.
    EXPECT_TRUE(quit_closure_.is_null());
  }

  void QuitAsyncRunLoop() {
    if (!IsAsync()) {
      EXPECT_TRUE(quit_closure_.is_null());
      return;
    }
    ASSERT_FALSE(quit_closure_.is_null());
    quit_closure_.Run();
    quit_closure_.Reset();
  }

  void MenuCloseCallback() {
    ++menu_close_count_;
    QuitAsyncRunLoop();
  }

  base::RepeatingClosure quit_closure_;
};

TEST_P(MenuRunnerCocoaTest, RunMenuAndCancel) {
  base::TimeTicks min_time = ui::EventTimeForNow();

  RunMenu(base::BindOnce(&MenuRunnerCocoaTest::MenuCancelCallback,
                         base::Unretained(this)));

  EXPECT_EQ(1, menu_close_count_);
  EXPECT_FALSE(runner_->IsRunning());

  if (GetParam() == MenuType::VIEWS) {
    // MenuItemView's MenuRunnerImpl gets the closing time from
    // MenuControllerCocoa:: closing_event_time(). This is is reset on show, but
    // only updated when an event closes the menu -- not a cancellation.
    EXPECT_EQ(runner_->GetClosingEventTime(), base::TimeTicks());
  } else {
    EXPECT_GE(runner_->GetClosingEventTime(), min_time);
  }
  EXPECT_LE(runner_->GetClosingEventTime(), ui::EventTimeForNow());

  // Cancel again.
  runner_->Cancel();
  EXPECT_FALSE(runner_->IsRunning());
  EXPECT_EQ(1, menu_close_count_);
}

TEST_P(MenuRunnerCocoaTest, RunMenuAndDelete) {
  RunMenu(base::BindOnce(&MenuRunnerCocoaTest::MenuDeleteCallback,
                         base::Unretained(this)));
  // Note the close callback is NOT invoked for deleted menus.
  EXPECT_EQ(0, menu_close_count_);
}

// Tests a potential lifetime issue using the Cocoa MenuController, which has a
// weak reference to the model.
TEST_P(MenuRunnerCocoaTest, RunMenuAndDeleteThenSelectItem) {
  RunMenu(
      base::BindOnce(&MenuRunnerCocoaTest::ModelDeleteThenSelectItemCallback,
                     base::Unretained(this)));
  EXPECT_EQ(0, menu_close_count_);
}

// Ensure a menu can be safely released immediately after a call to Cancel() in
// the same run loop iteration.
TEST_P(MenuRunnerCocoaTest, DestroyAfterCanceling) {
  RunMenu(base::BindOnce(&MenuRunnerCocoaTest::MenuCancelAndDeleteCallback,
                         base::Unretained(this)));

  if (IsAsync()) {
    EXPECT_EQ(1, menu_close_count_);
  } else {
    // For a synchronous menu, the deletion happens before the cancel can be
    // processed, so the close callback will not be invoked.
    EXPECT_EQ(0, menu_close_count_);
  }
}

TEST_P(MenuRunnerCocoaTest, RunMenuTwice) {
  for (int i = 0; i < 2; ++i) {
    RunMenu(base::BindOnce(&MenuRunnerCocoaTest::MenuCancelCallback,
                           base::Unretained(this)));
    EXPECT_FALSE(runner_->IsRunning());
    EXPECT_EQ(i + 1, menu_close_count_);
  }
}

TEST_P(MenuRunnerCocoaTest, CancelWithoutRunning) {
  runner_->Cancel();
  EXPECT_FALSE(runner_->IsRunning());
  EXPECT_EQ(base::TimeTicks(), runner_->GetClosingEventTime());
  EXPECT_EQ(0, menu_close_count_);
}

TEST_P(MenuRunnerCocoaTest, DeleteWithoutRunning) {
  runner_.ExtractAsDangling()->Release();
  EXPECT_EQ(0, menu_close_count_);
}

INSTANTIATE_TEST_SUITE_P(,
                         MenuRunnerCocoaTest,
                         ::testing::Values(MenuType::NATIVE, MenuType::VIEWS),
                         &MenuTypeToString);

}  // namespace views::test
