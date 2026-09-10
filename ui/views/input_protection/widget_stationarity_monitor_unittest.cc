// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_protection/widget_stationarity_monitor.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

namespace views::test {

namespace {

class TestStationarityObserver {
 public:
  // `base::Unretained` is safe because `this` owns the subscription.
  TestStationarityObserver()
      : stationarity_changed_subscription_(
            WidgetStationarityMonitor::GetInstance()
                .RegisterStationarityChangedCallbackForTesting(
                    base::BindRepeating(
                        &TestStationarityObserver::OnStationaryStateChanged,
                        base::Unretained(this)))) {}

  void OnStationaryStateChanged() { ++stationarity_change_count_; }

  int stationarity_change_count() const { return stationarity_change_count_; }
  void reset() { stationarity_change_count_ = 0; }

 private:
  int stationarity_change_count_ = 0;
  base::CallbackListSubscription stationarity_changed_subscription_;
};

}  // namespace

class WidgetStationarityMonitorTest : public WidgetTest {};

TEST_F(WidgetStationarityMonitorTest, TracksTopLevelBoundsChanges) {
  TestStationarityObserver observer;

  auto top_level = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                    Widget::InitParams::TYPE_WINDOW);
  WidgetStationarityMonitor::GetInstance().TrackWidget(*top_level);
  top_level->SetBounds(gfx::Rect(100, 100, 400, 300));
  top_level->Show();
  observer.reset();

  // Changing bounds on the tracked top-level widget must notify stationarity
  // observers.
  top_level->SetBounds(gfx::Rect(150, 150, 450, 350));
  EXPECT_GT(observer.stationarity_change_count(), 0);
}

TEST_F(WidgetStationarityMonitorTest, TracksTopLevelDestroying) {
  TestStationarityObserver observer;

  auto top_level = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                    Widget::InitParams::TYPE_WINDOW);
  WidgetStationarityMonitor::GetInstance().TrackWidget(*top_level);
  top_level->SetBounds(gfx::Rect(100, 100, 400, 300));
  top_level->Show();
  observer.reset();

  // Destroying a tracked top-level widget must notify stationarity observers.
  top_level->CloseNow();
  EXPECT_GT(observer.stationarity_change_count(), 0);
}

TEST_F(WidgetStationarityMonitorTest, IgnoresUntrackedTopLevelWidget) {
  TestStationarityObserver observer;

  auto top_level = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                    Widget::InitParams::TYPE_WINDOW);
  top_level->SetBounds(gfx::Rect(100, 100, 400, 300));
  top_level->Show();
  observer.reset();

  // Changing bounds or destroying an untracked widget must not notify
  // stationarity observers.
  top_level->SetBounds(gfx::Rect(150, 150, 450, 350));
  EXPECT_EQ(observer.stationarity_change_count(), 0);

  top_level->CloseNow();
  EXPECT_EQ(observer.stationarity_change_count(), 0);
}

TEST_F(WidgetStationarityMonitorTest,
       IgnoresChildWidgetBoundsChangesAndDestroying) {
  TestStationarityObserver observer;

  auto top_level = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                    Widget::InitParams::TYPE_WINDOW);
  WidgetStationarityMonitor::GetInstance().TrackWidget(*top_level);
  top_level->SetBounds(gfx::Rect(100, 100, 400, 300));
  top_level->Show();

  Widget* child = CreateChildPlatformWidget(top_level->GetNativeView());
  child->SetBounds(gfx::Rect(10, 10, 100, 100));
  child->Show();
  observer.reset();

  // Changing bounds on a child widget must not trigger stationarity changes.
  child->SetBounds(gfx::Rect(20, 20, 120, 120));
  EXPECT_EQ(observer.stationarity_change_count(), 0);

  // Destroying a child widget must not trigger stationarity changes.
  child->CloseNow();
  EXPECT_EQ(observer.stationarity_change_count(), 0);
}

TEST_F(WidgetStationarityMonitorTest,
       IgnoresModalDialogBoundsChangesAndDestroying) {
  TestStationarityObserver observer;

  auto top_level = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                    Widget::InitParams::TYPE_WINDOW);
  WidgetStationarityMonitor::GetInstance().TrackWidget(*top_level);
  top_level->SetBounds(gfx::Rect(100, 100, 400, 300));
  top_level->Show();

  DialogDelegate dialog_delegate;
  dialog_delegate.SetModalType(ui::mojom::ModalType::kChild);

  Widget* dialog_widget = DialogDelegate::CreateDialogWidget(
      &dialog_delegate, gfx::NativeWindow(), top_level->GetNativeView());
  dialog_widget->SetBounds(gfx::Rect(50, 50, 200, 150));
  dialog_widget->Show();
  observer.reset();

  // Changing bounds on a child-modal dialog must not trigger stationarity
  // changes.
  dialog_widget->SetBounds(gfx::Rect(50, 50, 200, 100));
  EXPECT_EQ(observer.stationarity_change_count(), 0);

  // Destroying a child-modal dialog must not trigger stationarity changes.
  dialog_widget->CloseNow();
  EXPECT_EQ(observer.stationarity_change_count(), 0);
}

TEST_F(WidgetStationarityMonitorTest,
       IgnoresFramelessTooltipBoundsChangesAndDestroying) {
  TestStationarityObserver observer;

  auto top_level = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                    Widget::InitParams::TYPE_WINDOW);
  WidgetStationarityMonitor::GetInstance().TrackWidget(*top_level);
  top_level->SetBounds(gfx::Rect(100, 100, 400, 300));
  top_level->Show();

  Widget::InitParams params = CreateParams(
      Widget::InitParams::CLIENT_OWNS_WIDGET, Widget::InitParams::TYPE_TOOLTIP);
  params.parent = top_level->GetNativeView();
  auto tooltip_widget = std::make_unique<Widget>();
  tooltip_widget->Init(std::move(params));
  tooltip_widget->Show();
  observer.reset();

  // Changing bounds on a frameless tooltip must not trigger stationarity
  // changes.
  tooltip_widget->SetBounds(gfx::Rect(10, 10, 100, 50));
  EXPECT_EQ(observer.stationarity_change_count(), 0);

  // Destroying a frameless tooltip must not trigger stationarity changes.
  tooltip_widget->CloseNow();
  EXPECT_EQ(observer.stationarity_change_count(), 0);
}

TEST_F(WidgetStationarityMonitorTest, IgnoresBubbleBoundsChangesAndDestroying) {
  TestStationarityObserver observer;

  auto top_level = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                    Widget::InitParams::TYPE_WINDOW);
  WidgetStationarityMonitor::GetInstance().TrackWidget(*top_level);
  top_level->SetBounds(gfx::Rect(100, 100, 400, 300));
  top_level->Show();

  auto bubble_delegate = std::make_unique<BubbleDialogDelegate>(
      top_level->GetContentsView(), BubbleBorder::Arrow::TOP_LEFT);
  std::unique_ptr<Widget> bubble_widget =
      BubbleDialogDelegate::CreateBubble(bubble_delegate.get());
  bubble_widget->SetBounds(gfx::Rect(50, 50, 200, 150));
  bubble_widget->Show();
  observer.reset();

  // Changing bounds on a bubble must not trigger stationarity changes.
  bubble_widget->SetBounds(gfx::Rect(50, 50, 200, 100));
  EXPECT_EQ(observer.stationarity_change_count(), 0);

  // Destroying a bubble must not trigger stationarity changes.
  bubble_widget->CloseNow();
  EXPECT_EQ(observer.stationarity_change_count(), 0);
}

TEST_F(WidgetStationarityMonitorTest, TracksMultipleTopLevelWidgets) {
  TestStationarityObserver observer;

  auto widget1 = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                  Widget::InitParams::TYPE_WINDOW);
  auto widget2 = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                  Widget::InitParams::TYPE_WINDOW);

  WidgetStationarityMonitor::GetInstance().TrackWidget(*widget1);
  WidgetStationarityMonitor::GetInstance().TrackWidget(*widget2);

  widget1->SetBounds(gfx::Rect(100, 100, 400, 300));
  widget1->Show();
  widget2->SetBounds(gfx::Rect(200, 200, 400, 300));
  widget2->Show();
  observer.reset();

  // Changing bounds on widget1 notifies observer.
  widget1->SetBounds(gfx::Rect(150, 150, 450, 350));
  EXPECT_GT(observer.stationarity_change_count(), 0);
  observer.reset();

  // Changing bounds on widget2 notifies observer.
  widget2->SetBounds(gfx::Rect(250, 250, 450, 350));
  EXPECT_GT(observer.stationarity_change_count(), 0);
  observer.reset();

  // Closing widget1 notifies observer and unobserves it.
  widget1->CloseNow();
  EXPECT_GT(observer.stationarity_change_count(), 0);
  observer.reset();

  // widget2 is still tracked and notifies on bounds change.
  widget2->SetBounds(gfx::Rect(300, 300, 450, 350));
  EXPECT_GT(observer.stationarity_change_count(), 0);
  observer.reset();

  // Closing widget2 notifies observer.
  widget2->CloseNow();
  EXPECT_GT(observer.stationarity_change_count(), 0);
}

TEST_F(WidgetStationarityMonitorTest, TrackWidgetIsIdempotent) {
  TestStationarityObserver observer;

  auto top_level = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                    Widget::InitParams::TYPE_WINDOW);

  // Tracking the same widget multiple times must not crash and be idempotent.
  WidgetStationarityMonitor::GetInstance().TrackWidget(*top_level);
  WidgetStationarityMonitor::GetInstance().TrackWidget(*top_level);

  top_level->SetBounds(gfx::Rect(100, 100, 400, 300));
  top_level->Show();
  observer.reset();

  top_level->SetBounds(gfx::Rect(150, 150, 450, 350));
  EXPECT_GT(observer.stationarity_change_count(), 0);

  top_level->CloseNow();
}

TEST_F(WidgetStationarityMonitorTest, SubscriptionUnregistration) {
  int change_count = 0;
  auto top_level = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                    Widget::InitParams::TYPE_WINDOW);
  WidgetStationarityMonitor::GetInstance().TrackWidget(*top_level);
  top_level->SetBounds(gfx::Rect(100, 100, 400, 300));
  top_level->Show();

  {
    // `base::Unretained` is safe because `change_count` outlives the scoped
    // subscription.
    base::CallbackListSubscription subscription =
        WidgetStationarityMonitor::GetInstance()
            .RegisterStationarityChangedCallbackForTesting(
                base::BindRepeating([](int* count) { ++(*count); },
                                    base::Unretained(&change_count)));

    top_level->SetBounds(gfx::Rect(150, 150, 450, 350));
    EXPECT_GT(change_count, 0);
  }

  // Once `subscription` is destroyed, no further notifications should fire.
  const int count_after_unregistration = change_count;

  top_level->SetBounds(gfx::Rect(200, 200, 450, 350));
  EXPECT_EQ(change_count, count_after_unregistration);

  top_level->CloseNow();
  EXPECT_EQ(change_count, count_after_unregistration);
}

TEST_F(WidgetStationarityMonitorTest, MultipleSubscribers) {
  // Simulate multiple subscribers registering callbacks.
  TestStationarityObserver observer1;
  TestStationarityObserver observer2;

  auto top_level = CreateTestWidget(Widget::InitParams::CLIENT_OWNS_WIDGET,
                                    Widget::InitParams::TYPE_WINDOW);
  WidgetStationarityMonitor::GetInstance().TrackWidget(*top_level);
  top_level->SetBounds(gfx::Rect(100, 100, 400, 300));
  top_level->Show();
  observer1.reset();
  observer2.reset();

  // Both subscribers receive notifications when the tracked widget changes
  // bounds.
  top_level->SetBounds(gfx::Rect(150, 150, 450, 350));
  EXPECT_GT(observer1.stationarity_change_count(), 0);
  EXPECT_GT(observer2.stationarity_change_count(), 0);
  EXPECT_EQ(observer1.stationarity_change_count(),
            observer2.stationarity_change_count());
  observer1.reset();
  observer2.reset();

  // Both subscribers receive notifications when the tracked widget closes.
  top_level->CloseNow();
  EXPECT_GT(observer1.stationarity_change_count(), 0);
  EXPECT_GT(observer2.stationarity_change_count(), 0);
  EXPECT_EQ(observer1.stationarity_change_count(),
            observer2.stationarity_change_count());
}

}  // namespace views::test
