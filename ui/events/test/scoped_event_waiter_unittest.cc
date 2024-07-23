// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/scoped_event_waiter.h"

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/test/test_event_processor.h"
#include "ui/events/test/test_event_target.h"
#include "ui/events/test/test_event_targeter.h"

namespace ui::test {

class ScopedEventWaiterTest : public testing::Test {
 public:
  ScopedEventWaiterTest() {
    auto root_target = std::make_unique<TestEventTarget>();
    root_target->SetEventTargeter(std::make_unique<TestEventTargeter>(
        root_target.get(), /*should_bubble=*/false));
    root_target_ = root_target.get();
    processor_.SetRoot(std::move(root_target));
  }
  ~ScopedEventWaiterTest() override = default;

  // Dispatches `event` to `root_target_` in the asynchronous manner.
  void DispatchEventAsync(ui::Event* event) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ScopedEventWaiterTest::DispatchEvent,
                                  base::Unretained(this), event));
  }

  TestEventTarget* root_target() { return root_target_; }

 private:
  void DispatchEvent(ui::Event* event) { processor_.OnEventFromSource(event); }

  TestEventProcessor processor_;
  raw_ptr<TestEventTarget> root_target_ = nullptr;
  base::test::SingleThreadTaskEnvironment task_environment;
};

// Waits until an event target receives the key pressed event.
TEST_F(ScopedEventWaiterTest, Basic) {
  ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_A, ui::EF_NONE);
  Event::DispatcherApi(&event).set_target(root_target());
  DispatchEventAsync(&event);
  ScopedEventWaiter(root_target(), ui::EventType::kKeyPressed).Wait();

  // Verify that the original event handler of the root target still receives
  // the event.
  EXPECT_TRUE(root_target()->DidReceiveEvent(event.type()));
}

}  // namespace ui::test
