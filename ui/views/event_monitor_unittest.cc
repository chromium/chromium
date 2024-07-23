// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/event_monitor.h"

#include <utility>

#include "base/memory/raw_ptr.h"
#include "ui/events/event_observer.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget_utils.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace views::test {

// A simple event observer that records the number of events.
class TestEventObserver : public ui::EventObserver {
 public:
  TestEventObserver() = default;

  TestEventObserver(const TestEventObserver&) = delete;
  TestEventObserver& operator=(const TestEventObserver&) = delete;

  ~TestEventObserver() override = default;

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override { ++observed_event_count_; }

  size_t observed_event_count() const { return observed_event_count_; }

 private:
  size_t observed_event_count_ = 0;
};

class EventMonitorTest : public WidgetTest {
 public:
  EventMonitorTest() = default;

  EventMonitorTest(const EventMonitorTest&) = delete;
  EventMonitorTest& operator=(const EventMonitorTest&) = delete;

  // testing::Test:
  void SetUp() override {
    WidgetTest::SetUp();
    widget_ = CreateTopLevelNativeWidget();
    widget_->SetSize(gfx::Size(100, 100));
    widget_->Show();
    generator_ = std::make_unique<ui::test::EventGenerator>(
        GetContext(), widget_->GetNativeWindow());
    generator_->set_target(ui::test::EventGenerator::Target::APPLICATION);
  }
  void TearDown() override {
    widget_.ExtractAsDangling()->CloseNow();
    WidgetTest::TearDown();
  }

 protected:
  raw_ptr<Widget> widget_ = nullptr;
  std::unique_ptr<ui::test::EventGenerator> generator_;
  TestEventObserver observer_;
};

TEST_F(EventMonitorTest, ShouldReceiveAppEventsWhileInstalled) {
  std::unique_ptr<EventMonitor> monitor(EventMonitor::CreateApplicationMonitor(
      &observer_, widget_->GetNativeWindow(),
      {ui::EventType::kMousePressed, ui::EventType::kMouseReleased}));

  generator_->ClickLeftButton();
  EXPECT_EQ(2u, observer_.observed_event_count());

  monitor.reset();
  generator_->ClickLeftButton();
  EXPECT_EQ(2u, observer_.observed_event_count());
}

TEST_F(EventMonitorTest, ShouldReceiveWindowEventsWhileInstalled) {
  std::unique_ptr<EventMonitor> monitor(EventMonitor::CreateWindowMonitor(
      &observer_, widget_->GetNativeWindow(),
      {ui::EventType::kMousePressed, ui::EventType::kMouseReleased}));

  generator_->ClickLeftButton();
  EXPECT_EQ(2u, observer_.observed_event_count());

  monitor.reset();
  generator_->ClickLeftButton();
  EXPECT_EQ(2u, observer_.observed_event_count());
}

TEST_F(EventMonitorTest, ShouldNotReceiveEventsFromOtherWindow) {
  Widget* widget2 = CreateTopLevelNativeWidget();
  std::unique_ptr<EventMonitor> monitor(EventMonitor::CreateWindowMonitor(
      &observer_, widget2->GetNativeWindow(),
      {ui::EventType::kMousePressed, ui::EventType::kMouseReleased}));

  generator_->ClickLeftButton();
  EXPECT_EQ(0u, observer_.observed_event_count());

  monitor.reset();
  widget2->CloseNow();
}

TEST_F(EventMonitorTest, ShouldOnlyReceiveRequestedEventTypes) {
  // This event monitor only listens to mouse press, not release.
  std::unique_ptr<EventMonitor> monitor(EventMonitor::CreateWindowMonitor(
      &observer_, widget_->GetNativeWindow(), {ui::EventType::kMousePressed}));

  generator_->ClickLeftButton();
  EXPECT_EQ(1u, observer_.observed_event_count());

  monitor.reset();
}

TEST_F(EventMonitorTest, WindowMonitorTornDownOnWindowClose) {
  Widget* widget2 = CreateTopLevelNativeWidget();
  widget2->Show();

  std::unique_ptr<EventMonitor> monitor(EventMonitor::CreateWindowMonitor(
      &observer_, widget2->GetNativeWindow(), {ui::EventType::kMousePressed}));

  // Closing the widget before destroying the monitor should not crash.
  widget2->CloseNow();
  monitor.reset();
}

namespace {
class DeleteOtherOnEventObserver : public ui::EventObserver {
 public:
  explicit DeleteOtherOnEventObserver(gfx::NativeWindow context) {
    monitor_ = EventMonitor::CreateApplicationMonitor(
        this, context,
        {ui::EventType::kMousePressed, ui::EventType::kMouseReleased});
  }

  DeleteOtherOnEventObserver(const DeleteOtherOnEventObserver&) = delete;
  DeleteOtherOnEventObserver& operator=(const DeleteOtherOnEventObserver&) =
      delete;

  bool DidDelete() const { return !observer_to_delete_; }

  void set_monitor_to_delete(
      std::unique_ptr<DeleteOtherOnEventObserver> observer_to_delete) {
    observer_to_delete_ = std::move(observer_to_delete);
  }

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override {
    observer_to_delete_ = nullptr;
  }

 private:
  std::unique_ptr<EventMonitor> monitor_;
  std::unique_ptr<DeleteOtherOnEventObserver> observer_to_delete_;
};
}  // namespace

// Ensure correct behavior when an event monitor is removed while iterating
// over the OS-controlled observer list.
TEST_F(EventMonitorTest, TwoMonitors) {
  gfx::NativeWindow window = widget_->GetNativeWindow();
  auto deleter = std::make_unique<DeleteOtherOnEventObserver>(window);
  auto deletee = std::make_unique<DeleteOtherOnEventObserver>(window);
  deleter->set_monitor_to_delete(std::move(deletee));

  EXPECT_FALSE(deleter->DidDelete());
  generator_->PressLeftButton();
  EXPECT_TRUE(deleter->DidDelete());

  // Now try setting up observers in the alternate order.
  deletee = std::make_unique<DeleteOtherOnEventObserver>(window);
  deleter = std::make_unique<DeleteOtherOnEventObserver>(window);
  deleter->set_monitor_to_delete(std::move(deletee));

  EXPECT_FALSE(deleter->DidDelete());
  generator_->ReleaseLeftButton();
  EXPECT_TRUE(deleter->DidDelete());
}

}  // namespace views::test
