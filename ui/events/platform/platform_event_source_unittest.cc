// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/platform/platform_event_source.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_observer.h"
#include "ui/events/platform/scoped_event_dispatcher.h"

namespace ui {

namespace {

void RemoveDispatcher(PlatformEventDispatcher* dispatcher) {
  PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(dispatcher);
}

void RemoveDispatchers(PlatformEventDispatcher* first,
                       PlatformEventDispatcher* second) {
  PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(first);
  PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(second);
}

void AddDispatcher(PlatformEventDispatcher* dispatcher) {
  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(dispatcher);
}

}  // namespace

class TestPlatformEventSource : public PlatformEventSource {
 public:
  TestPlatformEventSource() = default;
  TestPlatformEventSource(const TestPlatformEventSource&) = delete;
  TestPlatformEventSource& operator=(const TestPlatformEventSource&) = delete;
  ~TestPlatformEventSource() override = default;

  uint32_t Dispatch(const PlatformEvent& event) { return DispatchEvent(event); }
};

class TestPlatformEventDispatcher : public PlatformEventDispatcher {
 public:
  TestPlatformEventDispatcher(int id, std::vector<int>* list)
      : id_(id), list_(list) {
    PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);
  }

  TestPlatformEventDispatcher(const TestPlatformEventDispatcher&) = delete;
  TestPlatformEventDispatcher& operator=(const TestPlatformEventDispatcher&) =
      delete;

  ~TestPlatformEventDispatcher() override {
    PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  }

  void set_post_dispatch_action(uint32_t action) {
    post_dispatch_action_ = action;
  }

 protected:
  // PlatformEventDispatcher:
  bool CanDispatchEvent(const PlatformEvent& event) override { return true; }

  uint32_t DispatchEvent(const PlatformEvent& event) override {
    list_->push_back(id_);
    return post_dispatch_action_;
  }

 private:
  int id_;
  raw_ptr<std::vector<int>> list_;
  uint32_t post_dispatch_action_ = POST_DISPATCH_NONE;
};

class TestPlatformEventObserver : public PlatformEventObserver {
 public:
  TestPlatformEventObserver(int id, std::vector<int>* list)
      : id_(id), list_(list) {
    PlatformEventSource::GetInstance()->AddPlatformEventObserver(this);
  }

  TestPlatformEventObserver(const TestPlatformEventObserver&) = delete;
  TestPlatformEventObserver& operator=(const TestPlatformEventObserver&) =
      delete;

  ~TestPlatformEventObserver() override {
    PlatformEventSource::GetInstance()->RemovePlatformEventObserver(this);
  }

 protected:
  // PlatformEventObserver:
  void WillProcessEvent(const PlatformEvent& event) override {
    list_->push_back(id_);
  }

  void DidProcessEvent(const PlatformEvent& event) override {}

 private:
  int id_;
  raw_ptr<std::vector<int>> list_;
};

class PlatformEventTest : public testing::Test {
 public:
  PlatformEventTest() = default;

  PlatformEventTest(const PlatformEventTest&) = delete;
  PlatformEventTest& operator=(const PlatformEventTest&) = delete;

  ~PlatformEventTest() override = default;

  TestPlatformEventSource* source() { return source_.get(); }

 protected:
  // testing::Test:
  void SetUp() override {
    source_ = std::make_unique<TestPlatformEventSource>();
  }

 private:
  std::unique_ptr<TestPlatformEventSource> source_;
};

// Tests that a dispatcher receives an event.
TEST_F(PlatformEventTest, DispatcherBasic) {
  std::vector<int> list_dispatcher;
  std::optional<PlatformEvent> event(CreateInvalidPlatformEvent());
  source()->Dispatch(*event);
  EXPECT_EQ(0u, list_dispatcher.size());
  {
    TestPlatformEventDispatcher dispatcher(1, &list_dispatcher);

    event = CreateInvalidPlatformEvent();
    source()->Dispatch(*event);
    ASSERT_EQ(1u, list_dispatcher.size());
    EXPECT_EQ(1, list_dispatcher[0]);
  }

  list_dispatcher.clear();
  event = CreateInvalidPlatformEvent();
  source()->Dispatch(*event);
  EXPECT_EQ(0u, list_dispatcher.size());
}

// Tests that dispatchers receive events in the correct order.
TEST_F(PlatformEventTest, DispatcherOrder) {
  std::vector<int> list_dispatcher;
  int sequence[] = {21, 3, 6, 45};
  std::vector<std::unique_ptr<TestPlatformEventDispatcher>> dispatchers;
  for (auto id : sequence) {
    dispatchers.push_back(
        std::make_unique<TestPlatformEventDispatcher>(id, &list_dispatcher));
  }
  std::optional<PlatformEvent> event(CreateInvalidPlatformEvent());
  source()->Dispatch(*event);
  ASSERT_EQ(std::size(sequence), list_dispatcher.size());
  EXPECT_EQ(std::vector<int>(sequence, sequence + std::size(sequence)),
            list_dispatcher);
}

// Tests that if a dispatcher consumes the event, the subsequent dispatchers do
// not receive the event.
TEST_F(PlatformEventTest, DispatcherConsumesEventToStopDispatch) {
  std::vector<int> list_dispatcher;
  TestPlatformEventDispatcher first(12, &list_dispatcher);
  TestPlatformEventDispatcher second(23, &list_dispatcher);

  std::optional<PlatformEvent> event(CreateInvalidPlatformEvent());
  source()->Dispatch(*event);
  ASSERT_EQ(2u, list_dispatcher.size());
  EXPECT_EQ(12, list_dispatcher[0]);
  EXPECT_EQ(23, list_dispatcher[1]);
  list_dispatcher.clear();

  first.set_post_dispatch_action(POST_DISPATCH_STOP_PROPAGATION);
  event = CreateInvalidPlatformEvent();
  source()->Dispatch(*event);
  ASSERT_EQ(1u, list_dispatcher.size());
  EXPECT_EQ(12, list_dispatcher[0]);
}

// Tests that observers receive events.
TEST_F(PlatformEventTest, ObserverBasic) {
  std::vector<int> list_observer;
  std::optional<PlatformEvent> event(CreateInvalidPlatformEvent());
  source()->Dispatch(*event);
  EXPECT_EQ(0u, list_observer.size());
  {
    TestPlatformEventObserver observer(31, &list_observer);

    event = CreateInvalidPlatformEvent();
    source()->Dispatch(*event);
    ASSERT_EQ(1u, list_observer.size());
    EXPECT_EQ(31, list_observer[0]);
  }

  list_observer.clear();
  event = CreateInvalidPlatformEvent();
  source()->Dispatch(*event);
  EXPECT_EQ(0u, list_observer.size());
}

// Tests that observers receive events in the correct order.
TEST_F(PlatformEventTest, ObserverOrder) {
  std::vector<int> list_observer;
  const int sequence[] = {21, 3, 6, 45};
  std::vector<std::unique_ptr<TestPlatformEventObserver>> observers;
  for (auto id : sequence) {
    observers.push_back(
        std::make_unique<TestPlatformEventObserver>(id, &list_observer));
  }
  std::optional<PlatformEvent> event(CreateInvalidPlatformEvent());
  source()->Dispatch(*event);
  ASSERT_EQ(std::size(sequence), list_observer.size());
  EXPECT_EQ(std::vector<int>(sequence, sequence + std::size(sequence)),
            list_observer);
}

// Tests that observers and dispatchers receive events in the correct order.
TEST_F(PlatformEventTest, DispatcherAndObserverOrder) {
  std::vector<int> list;
  TestPlatformEventDispatcher first_d(12, &list);
  TestPlatformEventObserver first_o(10, &list);
  TestPlatformEventDispatcher second_d(23, &list);
  TestPlatformEventObserver second_o(20, &list);
  std::optional<PlatformEvent> event(CreateInvalidPlatformEvent());
  source()->Dispatch(*event);
  const int expected[] = {10, 20, 12, 23};
  EXPECT_EQ(std::vector<int>(expected, expected + std::size(expected)), list);
}

// Tests that an overridden dispatcher receives events before the default
// dispatchers.
TEST_F(PlatformEventTest, OverriddenDispatcherBasic) {
  std::vector<int> list;
  TestPlatformEventDispatcher dispatcher(10, &list);
  TestPlatformEventObserver observer(15, &list);
  std::optional<PlatformEvent> event(CreateInvalidPlatformEvent());
  source()->Dispatch(*event);
  ASSERT_EQ(2u, list.size());
  EXPECT_EQ(15, list[0]);
  EXPECT_EQ(10, list[1]);
  list.clear();

  TestPlatformEventDispatcher overriding_dispatcher(20, &list);
  source()->RemovePlatformEventDispatcher(&overriding_dispatcher);
  std::unique_ptr<ScopedEventDispatcher> handle =
      source()->OverrideDispatcher(&overriding_dispatcher);
  source()->Dispatch(*event);
  ASSERT_EQ(2u, list.size());
  EXPECT_EQ(15, list[0]);
  EXPECT_EQ(20, list[1]);
}

// Tests that an overridden dispatcher can request that the default dispatchers
// can dispatch the events.
TEST_F(PlatformEventTest, OverriddenDispatcherInvokeDefaultDispatcher) {
  std::vector<int> list;
  TestPlatformEventDispatcher dispatcher(10, &list);
  TestPlatformEventObserver observer(15, &list);
  TestPlatformEventDispatcher overriding_dispatcher(20, &list);
  source()->RemovePlatformEventDispatcher(&overriding_dispatcher);
  std::unique_ptr<ScopedEventDispatcher> handle =
      source()->OverrideDispatcher(&overriding_dispatcher);
  overriding_dispatcher.set_post_dispatch_action(POST_DISPATCH_PERFORM_DEFAULT);

  std::optional<PlatformEvent> event(CreateInvalidPlatformEvent());
  source()->Dispatch(*event);
  // First the observer, then the overriding dispatcher, then the default
  // dispatcher.
  ASSERT_EQ(3u, list.size());
  EXPECT_EQ(15, list[0]);
  EXPECT_EQ(20, list[1]);
  EXPECT_EQ(10, list[2]);
  list.clear();

  // Install a second overriding dispatcher.
  TestPlatformEventDispatcher second_overriding(50, &list);
  source()->RemovePlatformEventDispatcher(&second_overriding);
  std::unique_ptr<ScopedEventDispatcher> second_override_handle =
      source()->OverrideDispatcher(&second_overriding);
  source()->Dispatch(*event);
  ASSERT_EQ(2u, list.size());
  EXPECT_EQ(15, list[0]);
  EXPECT_EQ(50, list[1]);
  list.clear();

  second_overriding.set_post_dispatch_action(POST_DISPATCH_PERFORM_DEFAULT);
  source()->Dispatch(*event);
  // First the observer, then the second overriding dispatcher, then the default
  // dispatcher.
  ASSERT_EQ(3u, list.size());
  EXPECT_EQ(15, list[0]);
  EXPECT_EQ(50, list[1]);
  EXPECT_EQ(10, list[2]);
}

// Runs a callback during an event dispatch.
class RunCallbackDuringDispatch : public TestPlatformEventDispatcher {
 public:
  RunCallbackDuringDispatch(int id, std::vector<int>* list)
      : TestPlatformEventDispatcher(id, list) {}

  RunCallbackDuringDispatch(const RunCallbackDuringDispatch&) = delete;
  RunCallbackDuringDispatch& operator=(const RunCallbackDuringDispatch&) =
      delete;

  ~RunCallbackDuringDispatch() override {}

  void set_callback(base::OnceClosure callback) {
    callback_ = std::move(callback);
  }

 protected:
  // PlatformEventDispatcher:
  uint32_t DispatchEvent(const PlatformEvent& event) override {
    if (!callback_.is_null())
      std::move(callback_).Run();
    return TestPlatformEventDispatcher::DispatchEvent(event);
  }

 private:
  base::OnceClosure callback_;
};

// Test that if a dispatcher removes another dispatcher that is later in the
// dispatcher list during dispatching an event, then event dispatching still
// continues correctly.
TEST_F(PlatformEventTest, DispatcherRemovesNextDispatcherDuringDispatch) {
  std::vector<int> list;
  TestPlatformEventDispatcher first(10, &list);
  RunCallbackDuringDispatch second(15, &list);
  TestPlatformEventDispatcher third(20, &list);
  TestPlatformEventDispatcher fourth(30, &list);

  second.set_callback(
      base::BindOnce(&RemoveDispatcher, base::Unretained(&third)));

  std::optional<PlatformEvent> event(CreateInvalidPlatformEvent());
  source()->Dispatch(*event);
  // |second| removes |third| from the dispatcher list during dispatch. So the
  // event should only reach |first|, |second|, and |fourth|.
  ASSERT_EQ(3u, list.size());
  EXPECT_EQ(10, list[0]);
  EXPECT_EQ(15, list[1]);
  EXPECT_EQ(30, list[2]);
}

// Tests that if a dispatcher removes itself from the dispatcher list during
// dispatching an event, then event dispatching continues correctly.
TEST_F(PlatformEventTest, DispatcherRemovesSelfDuringDispatch) {
  std::vector<int> list;
  TestPlatformEventDispatcher first(10, &list);
  RunCallbackDuringDispatch second(15, &list);
  TestPlatformEventDispatcher third(20, &list);

  second.set_callback(
      base::BindOnce(&RemoveDispatcher, base::Unretained(&second)));

  std::optional<PlatformEvent> event(CreateInvalidPlatformEvent());
  source()->Dispatch(*event);
  // |second| removes itself from the dispatcher list during dispatch. So the
  // event should reach all three dispatchers in the list.
  ASSERT_EQ(3u, list.size());
  EXPECT_EQ(10, list[0]);
  EXPECT_EQ(15, list[1]);
  EXPECT_EQ(20, list[2]);
}

// Tests that if a dispatcher removes itself from the dispatcher list during
// dispatching an event, and this dispatcher is last in the dispatcher-list,
// then event dispatching ends correctly.
TEST_F(PlatformEventTest, DispatcherRemovesSelfDuringDispatchLast) {
  std::vector<int> list;
  TestPlatformEventDispatcher first(10, &list);
  RunCallbackDuringDispatch second(15, &list);

  second.set_callback(
      base::BindOnce(&RemoveDispatcher, base::Unretained(&second)));

  std::optional<PlatformEvent> event(CreateInvalidPlatformEvent());
  source()->Dispatch(*event);
  // |second| removes itself during dispatch. So both dispatchers will have
  // received the event.
  ASSERT_EQ(2u, list.size());
  EXPECT_EQ(10, list[0]);
  EXPECT_EQ(15, list[1]);
}

// Tests that if a dispatcher removes a single dispatcher that comes before it
// in the dispatcher list, then dispatch continues correctly.
TEST_F(PlatformEventTest, DispatcherRemovesPrevDispatcherDuringDispatch) {
  std::vector<int> list;
  TestPlatformEventDispatcher first(10, &list);
  RunCallbackDuringDispatch second(15, &list);
  TestPlatformEventDispatcher third(20, &list);

  second.set_callback(
      base::BindOnce(&RemoveDispatcher, base::Unretained(&first)));

  std::optional<PlatformEvent> event(CreateInvalidPlatformEvent());
  source()->Dispatch(*event);
  // |second| removes |first| from the dispatcher list during dispatch. The
  // event should reach all three dispatchers.
  ASSERT_EQ(3u, list.size());
  EXPECT_EQ(10, list[0]);
  EXPECT_EQ(15, list[1]);
  EXPECT_EQ(20, list[2]);
}

// Tests that if a dispatcher removes multiple dispatchers that comes before it
// in the dispatcher list, then dispatch continues correctly.
TEST_F(PlatformEventTest, DispatcherRemovesPrevDispatchersDuringDispatch) {
  std::vector<int> list;
  TestPlatformEventDispatcher first(10, &list);
  TestPlatformEventDispatcher second(12, &list);
  RunCallbackDuringDispatch third(15, &list);
  TestPlatformEventDispatcher fourth(20, &list);

  third.set_callback(base::BindOnce(
      &RemoveDispatchers, base::Unretained(&first), base::Unretained(&second)));

  std::optional<PlatformEvent> event(CreateInvalidPlatformEvent());
  source()->Dispatch(*event);
  // |third| removes |first| and |second| from the dispatcher list during
  // dispatch. The event should reach all three dispatchers.
  ASSERT_EQ(4u, list.size());
  EXPECT_EQ(10, list[0]);
  EXPECT_EQ(12, list[1]);
  EXPECT_EQ(15, list[2]);
  EXPECT_EQ(20, list[3]);
}

// Tests that adding a dispatcher during dispatching an event receives that
// event.
TEST_F(PlatformEventTest, DispatcherAddedDuringDispatchReceivesEvent) {
  std::vector<int> list;
  TestPlatformEventDispatcher first(10, &list);
  RunCallbackDuringDispatch second(15, &list);
  TestPlatformEventDispatcher third(20, &list);
  TestPlatformEventDispatcher fourth(30, &list);
  RemoveDispatchers(&third, &fourth);

  std::optional<PlatformEvent> event(CreateInvalidPlatformEvent());
  source()->Dispatch(*event);
  ASSERT_EQ(2u, list.size());
  EXPECT_EQ(10, list[0]);
  EXPECT_EQ(15, list[1]);

  second.set_callback(base::BindOnce(&AddDispatcher, base::Unretained(&third)));
  list.clear();
  source()->Dispatch(*event);
  ASSERT_EQ(3u, list.size());
  EXPECT_EQ(10, list[0]);
  EXPECT_EQ(15, list[1]);
  EXPECT_EQ(20, list[2]);

  second.set_callback(
      base::BindOnce(&AddDispatcher, base::Unretained(&fourth)));
  list.clear();
  source()->Dispatch(*event);
  ASSERT_EQ(4u, list.size());
  EXPECT_EQ(10, list[0]);
  EXPECT_EQ(15, list[1]);
  EXPECT_EQ(20, list[2]);
  EXPECT_EQ(30, list[3]);
}

// Provides mechanism for running tests from inside an active message-loop.
class PlatformEventTestWithMessageLoop : public PlatformEventTest {
 public:
  PlatformEventTestWithMessageLoop() {}

  PlatformEventTestWithMessageLoop(const PlatformEventTestWithMessageLoop&) =
      delete;
  PlatformEventTestWithMessageLoop& operator=(
      const PlatformEventTestWithMessageLoop&) = delete;

  ~PlatformEventTestWithMessageLoop() override {}

  void Run() {
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&PlatformEventTestWithMessageLoop::RunTestImpl,
                       base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

 protected:
  virtual void RunTestImpl() = 0;

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
};

#define RUN_TEST_IN_MESSAGE_LOOP(name) \
  TEST_F(name, Run) { Run(); }

// Tests that a ScopedEventDispatcher restores the previous dispatcher when
// destroyed.
class ScopedDispatcherRestoresAfterDestroy
    : public PlatformEventTestWithMessageLoop {
 public:
  // PlatformEventTestWithMessageLoop:
  void RunTestImpl() override {
    std::vector<int> list;
    TestPlatformEventDispatcher dispatcher(10, &list);
    TestPlatformEventObserver observer(15, &list);

    TestPlatformEventDispatcher first_overriding(20, &list);
    source()->RemovePlatformEventDispatcher(&first_overriding);
    std::unique_ptr<ScopedEventDispatcher> first_override_handle =
        source()->OverrideDispatcher(&first_overriding);

    // Install a second overriding dispatcher.
    TestPlatformEventDispatcher second_overriding(50, &list);
    source()->RemovePlatformEventDispatcher(&second_overriding);
    std::unique_ptr<ScopedEventDispatcher> second_override_handle =
        source()->OverrideDispatcher(&second_overriding);

    std::optional<PlatformEvent> event(CreateInvalidPlatformEvent());
    source()->Dispatch(*event);
    ASSERT_EQ(2u, list.size());
    EXPECT_EQ(15, list[0]);
    EXPECT_EQ(50, list[1]);
    list.clear();

    second_override_handle.reset();
    source()->Dispatch(*event);
    ASSERT_EQ(2u, list.size());
    EXPECT_EQ(15, list[0]);
    EXPECT_EQ(20, list[1]);
  }
};

RUN_TEST_IN_MESSAGE_LOOP(ScopedDispatcherRestoresAfterDestroy)

// This dispatcher destroys the handle to the ScopedEventDispatcher when
// dispatching an event.
class DestroyScopedHandleDispatcher : public TestPlatformEventDispatcher {
 public:
  DestroyScopedHandleDispatcher(int id, std::vector<int>* list)
      : TestPlatformEventDispatcher(id, list) {}

  DestroyScopedHandleDispatcher(const DestroyScopedHandleDispatcher&) = delete;
  DestroyScopedHandleDispatcher& operator=(
      const DestroyScopedHandleDispatcher&) = delete;

  ~DestroyScopedHandleDispatcher() override {}

  void SetScopedHandle(std::unique_ptr<ScopedEventDispatcher> handler) {
    handler_ = std::move(handler);
  }

  void set_callback(base::OnceClosure callback) {
    callback_ = std::move(callback);
  }

 private:
  // PlatformEventDispatcher:
  bool CanDispatchEvent(const PlatformEvent& event) override { return true; }

  uint32_t DispatchEvent(const PlatformEvent& event) override {
    handler_.reset();
    uint32_t action = TestPlatformEventDispatcher::DispatchEvent(event);
    if (!callback_.is_null()) {
      std::move(callback_).Run();
    }
    return action;
  }

  std::unique_ptr<ScopedEventDispatcher> handler_;
  base::OnceClosure callback_;
};

// Tests that resetting an overridden dispatcher, and installing another
// overridden dispatcher before the nested message-loop completely unwinds
// function correctly.
class ConsecutiveOverriddenDispatcherInTheSameMessageLoopIteration
    : public PlatformEventTestWithMessageLoop {
 public:
  void NestedTask(std::unique_ptr<ScopedEventDispatcher> dispatch_handle,
                  std::vector<int>* list) {
    std::optional<PlatformEvent> event(CreateInvalidPlatformEvent());
    source()->Dispatch(*event);
    ASSERT_EQ(2u, list->size());
    EXPECT_EQ(15, (*list)[0]);
    EXPECT_EQ(20, (*list)[1]);
    list->clear();

    // Reset the override dispatcher. This should restore the default
    // dispatcher.
    dispatch_handle.reset();
    source()->Dispatch(*event);
    ASSERT_EQ(2u, list->size());
    EXPECT_EQ(15, (*list)[0]);
    EXPECT_EQ(10, (*list)[1]);
    list->clear();

    // Install another override-dispatcher.
    DestroyScopedHandleDispatcher second_overriding(70, list);
    source()->RemovePlatformEventDispatcher(&second_overriding);
    std::unique_ptr<ScopedEventDispatcher> second_override_handle =
        source()->OverrideDispatcher(&second_overriding);

    source()->Dispatch(*event);
    ASSERT_EQ(2u, list->size());
    EXPECT_EQ(15, (*list)[0]);
    EXPECT_EQ(70, (*list)[1]);
    list->clear();

    second_overriding.SetScopedHandle(std::move(second_override_handle));
    second_overriding.set_post_dispatch_action(POST_DISPATCH_NONE);
    base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
    second_overriding.set_callback(run_loop.QuitClosure());
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(base::IgnoreResult(&TestPlatformEventSource::Dispatch),
                       base::Unretained(source()), *event));
    run_loop.Run();
    ASSERT_EQ(2u, list->size());
    EXPECT_EQ(15, (*list)[0]);
    EXPECT_EQ(70, (*list)[1]);
    list->clear();

    // Terminate the message-loop.
    run_loop_.Quit();
  }

  // PlatformEventTestWithMessageLoop:
  void RunTestImpl() override {
    std::vector<int> list;
    TestPlatformEventDispatcher dispatcher(10, &list);
    TestPlatformEventObserver observer(15, &list);

    TestPlatformEventDispatcher overriding(20, &list);
    source()->RemovePlatformEventDispatcher(&overriding);
    std::unique_ptr<ScopedEventDispatcher> override_handle =
        source()->OverrideDispatcher(&overriding);

    std::optional<PlatformEvent> event(CreateInvalidPlatformEvent());
    source()->Dispatch(*event);
    ASSERT_EQ(2u, list.size());
    EXPECT_EQ(15, list[0]);
    EXPECT_EQ(20, list[1]);
    list.clear();

    // Start a nested message-loop, and destroy |override_handle| in the nested
    // loop. That should terminate the nested loop, restore the previous
    // dispatchers, and return control to this function.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ConsecutiveOverriddenDispatcherInTheSameMessageLoopIteration::
                NestedTask,
            base::Unretained(this), std::move(override_handle),
            base::Unretained(&list)));
    run_loop_.Run();

    // Dispatching the event should now reach the default dispatcher.
    source()->Dispatch(*event);
    ASSERT_EQ(2u, list.size());
    EXPECT_EQ(15, list[0]);
    EXPECT_EQ(10, list[1]);
  }

 private:
  base::RunLoop run_loop_{base::RunLoop::Type::kNestableTasksAllowed};
};

RUN_TEST_IN_MESSAGE_LOOP(
    ConsecutiveOverriddenDispatcherInTheSameMessageLoopIteration)

}  // namespace ui
