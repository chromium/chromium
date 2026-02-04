// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/interaction/element_tracker_widget_state.h"

#include <memory>
#include <utility>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/expect_call_in_scope.h"
#include "ui/base/interaction/interaction_sequence_test_util.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/widget_test.h"

namespace views::internal {

class ElementTrackerWidgetStateTest
    : public ViewsTestBase,
      public ElementTrackerWidgetState::Delegate {
 public:
  ElementTrackerWidgetStateTest() = default;
  ~ElementTrackerWidgetStateTest() override = default;

  void SetUp() override {
    ViewsTestBase::SetUp();
    widget_ = CreateWidget();
  }

  void TearDown() override {
    widget_.reset();
    ViewsTestBase::TearDown();
  }

  std::unique_ptr<Widget> CreateWidget() {
    auto widget = std::make_unique<Widget>();
    Widget::InitParams params =
        CreateParams(Widget::InitParams::CLIENT_OWNS_WIDGET,
                     Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.bounds = gfx::Rect(0, 0, 650, 650);
    widget->Init(std::move(params));
    return widget;
  }

 protected:
  using WidgetVisibilityChangedCallback =
      base::RepeatingCallback<void(const Widget*, bool)>;
  using WidgetDestroyingCallback = base::RepeatingCallback<void(const Widget*)>;

  std::unique_ptr<Widget> widget_;
  base::RepeatingCallbackList<void(const Widget*, bool)>
      widget_visibility_changed_callbacks_;
  base::RepeatingCallbackList<void(const Widget*)> widget_destroying_callbacks_;

  void FlushEvents() {
    // Flush events a few times just for good measure.
    for (int i = 0; i < 3; ++i) {
      base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, run_loop.QuitClosure());
      run_loop.Run();
    }
  }

 private:
  // ElementTrackerWidgetState::Delegate:
  void OnWidgetVisibilityChanged(const Widget* widget, bool visible) override {
    widget_visibility_changed_callbacks_.Notify(widget, visible);
  }

  void OnWidgetDestroying(const Widget* widget) override {
    widget_destroying_callbacks_.Notify(widget);
  }
};

TEST_F(ElementTrackerWidgetStateTest, WidgetStartsInvisible) {
  ElementTrackerWidgetState state(*this, *widget_);
  EXPECT_FALSE(state.visible());
}

TEST_F(ElementTrackerWidgetStateTest, WidgetStartsVisible) {
  views::test::WidgetVisibleWaiter waiter(widget_.get());
  widget_->Show();
  waiter.Wait();

  ElementTrackerWidgetState state(*this, *widget_);
  EXPECT_TRUE(state.visible());
}

TEST_F(ElementTrackerWidgetStateTest, WidgetBecomesVisible) {
  UNCALLED_MOCK_CALLBACK(WidgetVisibilityChangedCallback, visibility_changed);
  UNCALLED_MOCK_CALLBACK(WidgetDestroyingCallback, destroying);
  auto visibility_subscription =
      widget_visibility_changed_callbacks_.Add(visibility_changed.Get());
  auto destroying_subscription =
      widget_destroying_callbacks_.Add(destroying.Get());

  ElementTrackerWidgetState state(*this, *widget_);
  EXPECT_ASYNC_CALL_IN_SCOPE(visibility_changed, Run(widget_.get(), true),
                             widget_->Show());
}

TEST_F(ElementTrackerWidgetStateTest, WidgetBecomesNotVisible) {
  UNCALLED_MOCK_CALLBACK(WidgetVisibilityChangedCallback, visibility_changed);
  UNCALLED_MOCK_CALLBACK(WidgetDestroyingCallback, destroying);
  auto visibility_subscription =
      widget_visibility_changed_callbacks_.Add(visibility_changed.Get());
  auto destroying_subscription =
      widget_destroying_callbacks_.Add(destroying.Get());

  views::test::WidgetVisibleWaiter waiter(widget_.get());
  widget_->Show();
  waiter.Wait();

  ElementTrackerWidgetState state(*this, *widget_);
  EXPECT_ASYNC_CALL_IN_SCOPE(visibility_changed, Run(widget_.get(), false),
                             widget_->Hide());
}

TEST_F(ElementTrackerWidgetStateTest, WidgetVisibilityChangesMultipleTimes) {
  UNCALLED_MOCK_CALLBACK(WidgetVisibilityChangedCallback, visibility_changed);
  UNCALLED_MOCK_CALLBACK(WidgetDestroyingCallback, destroying);
  auto visibility_subscription =
      widget_visibility_changed_callbacks_.Add(visibility_changed.Get());
  auto destroying_subscription =
      widget_destroying_callbacks_.Add(destroying.Get());

  ElementTrackerWidgetState state(*this, *widget_);
  EXPECT_ASYNC_CALL_IN_SCOPE(visibility_changed, Run(widget_.get(), true),
                             widget_->Show());
  EXPECT_ASYNC_CALL_IN_SCOPE(visibility_changed, Run(widget_.get(), false),
                             widget_->Hide());
  EXPECT_ASYNC_CALL_IN_SCOPE(visibility_changed, Run(widget_.get(), true),
                             widget_->Show());
  EXPECT_ASYNC_CALL_IN_SCOPE(visibility_changed, Run(widget_.get(), false),
                             widget_->Hide());
}

TEST_F(ElementTrackerWidgetStateTest, WidgetDestroyedWhileNotVisible) {
  UNCALLED_MOCK_CALLBACK(WidgetVisibilityChangedCallback, visibility_changed);
  UNCALLED_MOCK_CALLBACK(WidgetDestroyingCallback, destroying);
  auto visibility_subscription =
      widget_visibility_changed_callbacks_.Add(visibility_changed.Get());
  auto destroying_subscription =
      widget_destroying_callbacks_.Add(destroying.Get());

  ElementTrackerWidgetState state(*this, *widget_);
  EXPECT_ASYNC_CALL_IN_SCOPE(destroying, Run(widget_.get()), widget_->Close());
}

TEST_F(ElementTrackerWidgetStateTest, WidgetDestroyedWhileVisible) {
  UNCALLED_MOCK_CALLBACK(WidgetVisibilityChangedCallback, visibility_changed);
  UNCALLED_MOCK_CALLBACK(WidgetDestroyingCallback, destroying);
  auto visibility_subscription =
      widget_visibility_changed_callbacks_.Add(visibility_changed.Get());
  auto destroying_subscription =
      widget_destroying_callbacks_.Add(destroying.Get());

  ElementTrackerWidgetState state(*this, *widget_);
  EXPECT_ASYNC_CALL_IN_SCOPE(visibility_changed, Run(widget_.get(), true),
                             widget_->Show());

  // Closing the widget might or might not trigger a visibility change.
  EXPECT_CALL(visibility_changed, Run(widget_.get(), false))
      .Times(testing::AtMost(1));

  EXPECT_ASYNC_CALL_IN_SCOPE(destroying, Run(widget_.get()), widget_->Close());
}

TEST_F(ElementTrackerWidgetStateTest, MinimizeDoesNotChangeVisibility) {
  UNCALLED_MOCK_CALLBACK(WidgetVisibilityChangedCallback, visibility_changed);
  UNCALLED_MOCK_CALLBACK(WidgetDestroyingCallback, destroying);
  auto visibility_subscription =
      widget_visibility_changed_callbacks_.Add(visibility_changed.Get());
  auto destroying_subscription =
      widget_destroying_callbacks_.Add(destroying.Get());

  ElementTrackerWidgetState state(*this, *widget_);
  EXPECT_ASYNC_CALL_IN_SCOPE(visibility_changed, Run(widget_.get(), true),
                             widget_->Show());
  widget_->Minimize();
  FlushEvents();
}

TEST_F(ElementTrackerWidgetStateTest,
       MinimizeAndRestoreDoesNotChangeVisibility) {
  UNCALLED_MOCK_CALLBACK(WidgetVisibilityChangedCallback, visibility_changed);
  UNCALLED_MOCK_CALLBACK(WidgetDestroyingCallback, destroying);
  auto visibility_subscription =
      widget_visibility_changed_callbacks_.Add(visibility_changed.Get());
  auto destroying_subscription =
      widget_destroying_callbacks_.Add(destroying.Get());

  ElementTrackerWidgetState state(*this, *widget_);
  EXPECT_ASYNC_CALL_IN_SCOPE(visibility_changed, Run(widget_.get(), true),
                             widget_->Show());
  widget_->Minimize();
  FlushEvents();
  widget_->Restore();
  FlushEvents();
}

TEST_F(ElementTrackerWidgetStateTest, CloseWhileMinimized) {
  UNCALLED_MOCK_CALLBACK(WidgetVisibilityChangedCallback, visibility_changed);
  UNCALLED_MOCK_CALLBACK(WidgetDestroyingCallback, destroying);
  auto visibility_subscription =
      widget_visibility_changed_callbacks_.Add(visibility_changed.Get());
  auto destroying_subscription =
      widget_destroying_callbacks_.Add(destroying.Get());

  ElementTrackerWidgetState state(*this, *widget_);
  EXPECT_ASYNC_CALL_IN_SCOPE(visibility_changed, Run(widget_.get(), true),
                             widget_->Show());
  widget_->Minimize();
  FlushEvents();

  // Closing the widget might or might not trigger a visibility change.
  EXPECT_CALL(visibility_changed, Run(widget_.get(), false))
      .Times(testing::AtMost(1));

  EXPECT_ASYNC_CALL_IN_SCOPE(destroying, Run(widget_.get()), widget_->Close());
}

TEST_F(ElementTrackerWidgetStateTest, HideImmediatelyAfterMinimizeAndRestore) {
  UNCALLED_MOCK_CALLBACK(WidgetVisibilityChangedCallback, visibility_changed);
  UNCALLED_MOCK_CALLBACK(WidgetDestroyingCallback, destroying);
  auto visibility_subscription =
      widget_visibility_changed_callbacks_.Add(visibility_changed.Get());
  auto destroying_subscription =
      widget_destroying_callbacks_.Add(destroying.Get());

  ElementTrackerWidgetState state(*this, *widget_);
  EXPECT_ASYNC_CALL_IN_SCOPE(visibility_changed, Run(widget_.get(), true),
                             widget_->Show());
  widget_->Minimize();
  FlushEvents();
  widget_->Restore();
  EXPECT_ASYNC_CALL_IN_SCOPE(visibility_changed, Run(widget_.get(), false),
                             widget_->Hide());
}

}  // namespace views::internal
