// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/events/event_dispatcher.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/event_target.h"
#include "ui/events/event_target_iterator.h"
#include "ui/events/event_utils.h"

namespace ui {

namespace {

class TestTarget : public EventTarget,
                   public EventHandler {
 public:
  TestTarget() : parent_(nullptr), valid_(true) { SetTargetHandler(this); }

  TestTarget(const TestTarget&) = delete;
  TestTarget& operator=(const TestTarget&) = delete;

  ~TestTarget() override {}

  void set_parent(TestTarget* parent) { parent_ = parent; }

  bool valid() const { return valid_; }
  void set_valid(bool valid) { valid_ = valid; }

  void AddHandlerId(int id) {
    handler_list_.push_back(id);
  }

  const std::vector<int>& handler_list() const { return handler_list_; }

  void Reset() {
    handler_list_.clear();
    valid_ = true;
  }

 private:
  // Overridden from EventTarget:
  bool CanAcceptEvent(const ui::Event& event) override { return true; }

  EventTarget* GetParentTarget() override { return parent_; }

  std::unique_ptr<EventTargetIterator> GetChildIterator() const override {
    return nullptr;
  }

  EventTargeter* GetEventTargeter() override { return nullptr; }

  raw_ptr<TestTarget> parent_;
  std::vector<int> handler_list_;
  bool valid_;
};

class TestEventHandler : public EventHandler {
 public:
  explicit TestEventHandler(int id) : id_(id) {}

  TestEventHandler(const TestEventHandler&) = delete;
  TestEventHandler& operator=(const TestEventHandler&) = delete;

  ~TestEventHandler() override {
    if (pre_target_)
      pre_target_->RemovePreTargetHandler(this);
  }

  void set_pre_target(EventTarget* pre_target) { pre_target_ = pre_target; }

  virtual void ReceivedEvent(Event* event) {
    static_cast<TestTarget*>(event->target())->AddHandlerId(id_);
    if (event->phase() == ui::EP_POSTTARGET) {
      EXPECT_TRUE(expect_post_target_);
      if (expect_pre_target_)
        EXPECT_TRUE(received_pre_target_);
    } else if (event->phase() == ui::EP_PRETARGET) {
      EXPECT_TRUE(expect_pre_target_);
      received_pre_target_ = true;
    } else {
      NOTREACHED_IN_MIGRATION();
    }
  }

  void set_event_result(EventResult result) { event_result_ = result; }

  void set_expect_pre_target(bool expect) { expect_pre_target_ = expect; }
  void set_expect_post_target(bool expect) { expect_post_target_ = expect; }

 private:
  // Overridden from EventHandler:
  void OnEvent(Event* event) override {
    ui::EventHandler::OnEvent(event);
    ReceivedEvent(event);
    SetStatusOnEvent(event);
  }

  void SetStatusOnEvent(Event* event) {
    if (event_result_ & ui::ER_CONSUMED)
      event->StopPropagation();
    if (event_result_ & ui::ER_HANDLED)
      event->SetHandled();
  }

  const int id_;
  EventResult event_result_ = ER_UNHANDLED;
  bool expect_pre_target_ = false;
  bool expect_post_target_ = false;
  bool received_pre_target_ = false;
  raw_ptr<EventTarget> pre_target_ = nullptr;
};

typedef CancelModeEvent NonCancelableEvent;

// Destroys the dispatcher-delegate when it receives any event.
class EventHandlerDestroyDispatcherDelegate : public TestEventHandler {
 public:
  EventHandlerDestroyDispatcherDelegate(
      std::unique_ptr<EventDispatcherDelegate> delegate,
      int id)
      : TestEventHandler(id), dispatcher_delegate_(std::move(delegate)) {}

  EventHandlerDestroyDispatcherDelegate(
      const EventHandlerDestroyDispatcherDelegate&) = delete;
  EventHandlerDestroyDispatcherDelegate& operator=(
      const EventHandlerDestroyDispatcherDelegate&) = delete;

  ~EventHandlerDestroyDispatcherDelegate() override {}

 private:
  void ReceivedEvent(Event* event) override {
    TestEventHandler::ReceivedEvent(event);
    dispatcher_delegate_.reset();
  }

  std::unique_ptr<EventDispatcherDelegate> dispatcher_delegate_;
};

// Invalidates the target when it receives any event.
class InvalidateTargetEventHandler : public TestEventHandler {
 public:
  explicit InvalidateTargetEventHandler(int id) : TestEventHandler(id) {}

  InvalidateTargetEventHandler(const InvalidateTargetEventHandler&) = delete;
  InvalidateTargetEventHandler& operator=(const InvalidateTargetEventHandler&) =
      delete;

  ~InvalidateTargetEventHandler() override {}

 private:
  void ReceivedEvent(Event* event) override {
   TestEventHandler::ReceivedEvent(event);
   TestTarget* target = static_cast<TestTarget*>(event->target());
   target->set_valid(false);
  }
};

// Destroys a second event handler when this handler gets an event.
// Optionally also destroys the dispatcher.
class EventHandlerDestroyer : public TestEventHandler {
 public:
  EventHandlerDestroyer(int id, std::unique_ptr<EventHandler> destroy)
      : TestEventHandler(id), to_destroy_(std::move(destroy)) {}

  EventHandlerDestroyer(const EventHandlerDestroyer&) = delete;
  EventHandlerDestroyer& operator=(const EventHandlerDestroyer&) = delete;

  ~EventHandlerDestroyer() override { CHECK(!to_destroy_); }

  void set_dispatcher_delegate(
      std::unique_ptr<EventDispatcherDelegate> dispatcher_delegate) {
   dispatcher_delegate_ = std::move(dispatcher_delegate);
  }

 private:
  void ReceivedEvent(Event* event) override {
    TestEventHandler::ReceivedEvent(event);
    to_destroy_.reset();
    dispatcher_delegate_.reset();
  }

  std::unique_ptr<EventHandler> to_destroy_;
  std::unique_ptr<EventDispatcherDelegate> dispatcher_delegate_;
};

class TestEventDispatcher : public EventDispatcherDelegate {
 public:
  TestEventDispatcher() {}

  TestEventDispatcher(const TestEventDispatcher&) = delete;
  TestEventDispatcher& operator=(const TestEventDispatcher&) = delete;

  ~TestEventDispatcher() override {}

  EventDispatchDetails ProcessEvent(EventTarget* target, Event* event) {
    return DispatchEvent(target, event);
  }

 private:
  // Overridden from EventDispatcherDelegate:
  bool CanDispatchToTarget(EventTarget* target) override {
    TestTarget* test_target = static_cast<TestTarget*>(target);
    return test_target->valid();
  }
};

}  // namespace

TEST(EventDispatcherTest, EventDispatchOrder) {
  TestEventDispatcher dispatcher;
  TestTarget parent, child;
  TestEventHandler h1(1), h2(2), h3(3), h4(4);
  TestEventHandler h5(5), h6(6), h7(7), h8(8);

  child.set_parent(&parent);

  parent.AddPreTargetHandler(&h1);
  parent.AddPreTargetHandler(&h2);

  child.AddPreTargetHandler(&h3);
  child.AddPreTargetHandler(&h4);

  h1.set_expect_pre_target(true);
  h2.set_expect_pre_target(true);
  h3.set_expect_pre_target(true);
  h4.set_expect_pre_target(true);

  child.AddPostTargetHandler(&h5);
  child.AddPostTargetHandler(&h6);

  parent.AddPostTargetHandler(&h7);
  parent.AddPostTargetHandler(&h8);

  h5.set_expect_post_target(true);
  h6.set_expect_post_target(true);
  h7.set_expect_post_target(true);
  h8.set_expect_post_target(true);

  MouseEvent mouse(ui::EventType::kMouseMoved, gfx::Point(3, 4),
                   gfx::Point(3, 4), ui::EventTimeForNow(), 0, 0);
  Event::DispatcherApi event_mod(&mouse);
  dispatcher.ProcessEvent(&child, &mouse);
  EXPECT_FALSE(mouse.stopped_propagation());
  EXPECT_FALSE(mouse.handled());

  {
    int expected[] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    EXPECT_EQ(
        std::vector<int>(expected, expected + sizeof(expected) / sizeof(int)),
        child.handler_list());
  }

  child.Reset();
  event_mod.set_phase(EP_PREDISPATCH);
  event_mod.set_result(ER_UNHANDLED);

  h1.set_event_result(ER_HANDLED);
  dispatcher.ProcessEvent(&child, &mouse);
  EXPECT_EQ(EP_POSTDISPATCH, mouse.phase());
  EXPECT_FALSE(mouse.stopped_propagation());
  EXPECT_TRUE(mouse.handled());
  {
    // |h1| marks the event as handled. So only the pre-target handlers should
    // receive the event.
    int expected[] = { 1, 2, 3, 4 };
    EXPECT_EQ(
        std::vector<int>(expected, expected + sizeof(expected) / sizeof(int)),
        child.handler_list());
  }

  child.Reset();
  event_mod.set_phase(EP_PREDISPATCH);
  event_mod.set_result(ER_UNHANDLED);

  int nexpected[] = { 1, 2, 3, 4, 5 };
  h1.set_event_result(ER_UNHANDLED);
  h5.set_event_result(ER_CONSUMED);
  dispatcher.ProcessEvent(&child, &mouse);
  EXPECT_EQ(EP_POSTDISPATCH, mouse.phase());
  EXPECT_TRUE(mouse.stopped_propagation());
  EXPECT_TRUE(mouse.handled());
  EXPECT_EQ(
      std::vector<int>(nexpected, nexpected + sizeof(nexpected) / sizeof(int)),
      child.handler_list());

  child.Reset();
  event_mod.set_phase(EP_PREDISPATCH);
  event_mod.set_result(ER_UNHANDLED);

  int exp[] = { 1 };
  h1.set_event_result(ER_CONSUMED);
  dispatcher.ProcessEvent(&child, &mouse);
  EXPECT_EQ(EP_POSTDISPATCH, mouse.phase());
  EXPECT_TRUE(mouse.stopped_propagation());
  EXPECT_TRUE(mouse.handled());
  EXPECT_EQ(
      std::vector<int>(exp, exp + sizeof(exp) / sizeof(int)),
      child.handler_list());

  parent.RemovePreTargetHandler(&h1);
  parent.RemovePreTargetHandler(&h2);

  child.RemovePreTargetHandler(&h3);
  child.RemovePreTargetHandler(&h4);
}

// Tests that the event-phases are correct.
TEST(EventDispatcherTest, EventDispatchPhase) {
  TestEventDispatcher dispatcher;
  TestTarget target;

  TestEventHandler handler(11);

  target.AddPreTargetHandler(&handler);
  target.AddPostTargetHandler(&handler);
  handler.set_expect_pre_target(true);
  handler.set_expect_post_target(true);

  MouseEvent mouse(ui::EventType::kMouseMoved, gfx::Point(3, 4),
                   gfx::Point(3, 4), ui::EventTimeForNow(), 0, 0);
  Event::DispatcherApi event_mod(&mouse);
  dispatcher.ProcessEvent(&target, &mouse);
  EXPECT_EQ(ER_UNHANDLED, mouse.result());

  int handlers[] = { 11, 11 };
  EXPECT_EQ(
      std::vector<int>(handlers, handlers + sizeof(handlers) / sizeof(int)),
      target.handler_list());

  target.RemovePreTargetHandler(&handler);
}

// Tests that if the dispatcher is destroyed in the middle of pre or post-target
// dispatching events, it doesn't cause a crash.
TEST(EventDispatcherTest, EventDispatcherDestroyedDuringDispatch) {
  // Test for pre-target first.
  {
    auto owned_dispatcher = std::make_unique<TestEventDispatcher>();
    TestEventDispatcher* dispatcher = owned_dispatcher.get();
    EventHandlerDestroyDispatcherDelegate handler(std::move(owned_dispatcher),
                                                  5);
    TestEventHandler h1(1);
    TestEventHandler h2(2);

    TestTarget target;
    target.AddPreTargetHandler(&h1);
    target.AddPreTargetHandler(&handler);
    target.AddPreTargetHandler(&h2);

    h1.set_expect_pre_target(true);
    handler.set_expect_pre_target(true);
    // |h2| should not receive any events at all since |handler| will have
    // destroyed the dispatcher.
    h2.set_expect_pre_target(false);

    MouseEvent mouse(ui::EventType::kMouseMoved, gfx::Point(3, 4),
                     gfx::Point(3, 4), ui::EventTimeForNow(), 0, 0);
    EventDispatchDetails details = dispatcher->ProcessEvent(&target, &mouse);
    EXPECT_TRUE(details.dispatcher_destroyed);
    EXPECT_EQ(ER_CONSUMED, mouse.result());
    EXPECT_EQ(2U, target.handler_list().size());
    EXPECT_EQ(1, target.handler_list()[0]);
    EXPECT_EQ(5, target.handler_list()[1]);

    target.RemovePreTargetHandler(&h1);
    target.RemovePreTargetHandler(&handler);
    target.RemovePreTargetHandler(&h2);
  }

  // Test for non-cancelable event.
  {
    auto owned_dispatcher = std::make_unique<TestEventDispatcher>();
    TestEventDispatcher* dispatcher = owned_dispatcher.get();
    EventHandlerDestroyDispatcherDelegate handler(std::move(owned_dispatcher),
                                                  5);
    TestEventHandler h1(1);
    TestEventHandler h2(2);

    TestTarget target;
    target.AddPreTargetHandler(&h1);
    target.AddPreTargetHandler(&handler);
    target.AddPreTargetHandler(&h2);

    h1.set_expect_pre_target(true);
    handler.set_expect_pre_target(true);
    // |h2| should not receive any events at all since |handler| will have
    // destroyed the dispatcher.
    h2.set_expect_pre_target(false);

    NonCancelableEvent event;
    EventDispatchDetails details = dispatcher->ProcessEvent(&target, &event);
    EXPECT_TRUE(details.dispatcher_destroyed);
    EXPECT_EQ(2U, target.handler_list().size());
    EXPECT_EQ(1, target.handler_list()[0]);
    EXPECT_EQ(5, target.handler_list()[1]);

    target.RemovePreTargetHandler(&h1);
    target.RemovePreTargetHandler(&handler);
    target.RemovePreTargetHandler(&h2);
  }

  // Now test for post-target.
  {
    auto owned_dispatcher = std::make_unique<TestEventDispatcher>();
    TestEventDispatcher* dispatcher = owned_dispatcher.get();
    EventHandlerDestroyDispatcherDelegate handler(std::move(owned_dispatcher),
                                                  5);
    TestEventHandler h1(1);
    TestEventHandler h2(2);

    TestTarget target;
    target.AddPostTargetHandler(&h1);
    target.AddPostTargetHandler(&handler);
    target.AddPostTargetHandler(&h2);

    h1.set_expect_post_target(true);
    handler.set_expect_post_target(true);
    // |h2| should not receive any events at all since |handler| will have
    // destroyed the dispatcher.
    h2.set_expect_post_target(false);

    MouseEvent mouse(ui::EventType::kMouseMoved, gfx::Point(3, 4),
                     gfx::Point(3, 4), ui::EventTimeForNow(), 0, 0);
    EventDispatchDetails details = dispatcher->ProcessEvent(&target, &mouse);
    EXPECT_TRUE(details.dispatcher_destroyed);
    EXPECT_EQ(ER_CONSUMED, mouse.result());
    EXPECT_EQ(2U, target.handler_list().size());
    EXPECT_EQ(1, target.handler_list()[0]);
    EXPECT_EQ(5, target.handler_list()[1]);

    target.RemovePostTargetHandler(&h1);
    target.RemovePostTargetHandler(&handler);
    target.RemovePostTargetHandler(&h2);
  }

  // Test for non-cancelable event.
  {
    auto owned_dispatcher = std::make_unique<TestEventDispatcher>();
    TestEventDispatcher* dispatcher = owned_dispatcher.get();
    EventHandlerDestroyDispatcherDelegate handler(std::move(owned_dispatcher),
                                                  5);
    TestEventHandler h1(1);
    TestEventHandler h2(2);

    TestTarget target;
    target.AddPostTargetHandler(&h1);
    target.AddPostTargetHandler(&handler);
    target.AddPostTargetHandler(&h2);

    h1.set_expect_post_target(true);
    handler.set_expect_post_target(true);
    // |h2| should not receive any events at all since |handler| will have
    // destroyed the dispatcher.
    h2.set_expect_post_target(false);

    NonCancelableEvent event;
    EventDispatchDetails details = dispatcher->ProcessEvent(&target, &event);
    EXPECT_TRUE(details.dispatcher_destroyed);
    EXPECT_EQ(2U, target.handler_list().size());
    EXPECT_EQ(1, target.handler_list()[0]);
    EXPECT_EQ(5, target.handler_list()[1]);

    target.RemovePostTargetHandler(&h1);
    target.RemovePostTargetHandler(&handler);
    target.RemovePostTargetHandler(&h2);
  }
}

// Tests that a target becoming invalid in the middle of pre- or post-target
// event processing aborts processing.
TEST(EventDispatcherTest, EventDispatcherInvalidateTarget) {
  TestEventDispatcher dispatcher;
  TestTarget target;
  TestEventHandler h1(1);
  InvalidateTargetEventHandler invalidate_handler(2);
  TestEventHandler h3(3);

  target.AddPreTargetHandler(&h1);
  target.AddPreTargetHandler(&invalidate_handler);
  target.AddPreTargetHandler(&h3);

  h1.set_expect_pre_target(true);
  invalidate_handler.set_expect_pre_target(true);
  // |h3| should not receive events as the target will be invalidated.
  h3.set_expect_pre_target(false);

  MouseEvent mouse(ui::EventType::kMouseMoved, gfx::Point(3, 4),
                   gfx::Point(3, 4), ui::EventTimeForNow(), 0, 0);
  EventDispatchDetails details = dispatcher.ProcessEvent(&target, &mouse);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_TRUE(details.target_destroyed);
  EXPECT_FALSE(target.valid());
  EXPECT_TRUE(mouse.stopped_propagation());
  EXPECT_EQ(2U, target.handler_list().size());
  EXPECT_EQ(1, target.handler_list()[0]);
  EXPECT_EQ(2, target.handler_list()[1]);

  // Test for non-cancelable event.
  target.Reset();
  NonCancelableEvent event;
  details = dispatcher.ProcessEvent(&target, &event);
  EXPECT_FALSE(details.dispatcher_destroyed);
  EXPECT_TRUE(details.target_destroyed);
  EXPECT_FALSE(target.valid());
  EXPECT_EQ(2U, target.handler_list().size());
  EXPECT_EQ(1, target.handler_list()[0]);
  EXPECT_EQ(2, target.handler_list()[1]);

  target.RemovePreTargetHandler(&h1);
  target.RemovePreTargetHandler(&invalidate_handler);
  target.RemovePreTargetHandler(&h3);
}

// Tests that if an event-handler gets destroyed during event-dispatch, it does
// not cause a crash.
TEST(EventDispatcherTest, EventHandlerDestroyedDuringDispatch) {
  {
    TestEventDispatcher dispatcher;
    TestEventHandler h1(1);
    auto owned_h3 = std::make_unique<TestEventHandler>(3);
    TestEventHandler* h3 = owned_h3.get();
    EventHandlerDestroyer handle_destroyer(2, std::move(owned_h3));

    TestTarget target;
    target.AddPreTargetHandler(&h1);
    target.AddPreTargetHandler(&handle_destroyer);
    h3->set_pre_target(&target);
    target.AddPreTargetHandler(h3);

    h1.set_expect_pre_target(true);
    handle_destroyer.set_expect_pre_target(true);
    // |h3| should not receive events since |handle_destroyer| will have
    // destroyed it.
    h3->set_expect_pre_target(false);

    MouseEvent mouse(ui::EventType::kMouseMoved, gfx::Point(3, 4),
                     gfx::Point(3, 4), ui::EventTimeForNow(), 0, 0);
    EventDispatchDetails details = dispatcher.ProcessEvent(&target, &mouse);
    EXPECT_FALSE(details.dispatcher_destroyed);
    EXPECT_FALSE(details.target_destroyed);
    EXPECT_FALSE(mouse.stopped_propagation());
    EXPECT_EQ(2U, target.handler_list().size());
    EXPECT_EQ(1, target.handler_list()[0]);
    EXPECT_EQ(2, target.handler_list()[1]);

    target.RemovePreTargetHandler(&h1);
    target.RemovePreTargetHandler(&handle_destroyer);
  }

  // Test for non-cancelable events.
  {
    TestEventDispatcher dispatcher;
    TestEventHandler h1(1);
    auto owned_h3 = std::make_unique<TestEventHandler>(3);
    TestEventHandler* h3 = owned_h3.get();
    EventHandlerDestroyer handle_destroyer(2, std::move(owned_h3));

    TestTarget target;
    target.AddPreTargetHandler(&h1);
    target.AddPreTargetHandler(&handle_destroyer);
    target.AddPreTargetHandler(h3);
    h3->set_pre_target(&target);

    h1.set_expect_pre_target(true);
    handle_destroyer.set_expect_pre_target(true);
    h3->set_expect_pre_target(false);

    NonCancelableEvent event;
    EventDispatchDetails details = dispatcher.ProcessEvent(&target, &event);
    EXPECT_FALSE(details.dispatcher_destroyed);
    EXPECT_FALSE(details.target_destroyed);
    EXPECT_EQ(2U, target.handler_list().size());
    EXPECT_EQ(1, target.handler_list()[0]);
    EXPECT_EQ(2, target.handler_list()[1]);

    target.RemovePreTargetHandler(&h1);
    target.RemovePreTargetHandler(&handle_destroyer);
  }
}

// Tests that things work correctly if an event-handler destroys both the
// dispatcher and a handler.
TEST(EventDispatcherTest, EventHandlerAndDispatcherDestroyedDuringDispatch) {
  {
    auto owned_dispatcher = std::make_unique<TestEventDispatcher>();
    TestEventDispatcher* dispatcher = owned_dispatcher.get();
    TestEventHandler h1(1);
    auto owned_h3 = std::make_unique<TestEventHandler>(3);
    TestEventHandler* h3 = owned_h3.get();
    EventHandlerDestroyer destroyer(2, std::move(owned_h3));

    TestTarget target;
    target.AddPreTargetHandler(&h1);
    target.AddPreTargetHandler(&destroyer);
    target.AddPreTargetHandler(h3);
    h3->set_pre_target(&target);
    h1.set_expect_pre_target(true);
    destroyer.set_expect_pre_target(true);
    destroyer.set_dispatcher_delegate(std::move(owned_dispatcher));
    // |h3| should not receive events since |destroyer| will have destroyed
    // it.
    h3->set_expect_pre_target(false);

    MouseEvent mouse(ui::EventType::kMouseMoved, gfx::Point(3, 4),
                     gfx::Point(3, 4), ui::EventTimeForNow(), 0, 0);
    EventDispatchDetails details = dispatcher->ProcessEvent(&target, &mouse);
    EXPECT_TRUE(details.dispatcher_destroyed);
    EXPECT_TRUE(mouse.stopped_propagation());
    EXPECT_EQ(2U, target.handler_list().size());
    EXPECT_EQ(1, target.handler_list()[0]);
    EXPECT_EQ(2, target.handler_list()[1]);

    target.RemovePreTargetHandler(&h1);
    target.RemovePreTargetHandler(&destroyer);
  }

  // Test for non-cancelable events.
  {
    auto owned_dispatcher = std::make_unique<TestEventDispatcher>();
    TestEventDispatcher* dispatcher = owned_dispatcher.get();
    TestEventHandler h1(1);
    auto owned_h3 = std::make_unique<TestEventHandler>(3);
    TestEventHandler* h3 = owned_h3.get();
    EventHandlerDestroyer destroyer(2, std::move(owned_h3));

    TestTarget target;
    target.AddPreTargetHandler(&h1);
    target.AddPreTargetHandler(&destroyer);
    target.AddPreTargetHandler(h3);
    h3->set_pre_target(&target);

    h1.set_expect_pre_target(true);
    destroyer.set_expect_pre_target(true);
    destroyer.set_dispatcher_delegate(std::move(owned_dispatcher));
    // |h3| should not receive events since |destroyer| will have destroyed
    // it.
    h3->set_expect_pre_target(false);

    NonCancelableEvent event;
    EventDispatchDetails details = dispatcher->ProcessEvent(&target, &event);
    EXPECT_TRUE(details.dispatcher_destroyed);
    EXPECT_EQ(2U, target.handler_list().size());
    EXPECT_EQ(1, target.handler_list()[0]);
    EXPECT_EQ(2, target.handler_list()[1]);

    target.RemovePreTargetHandler(&h1);
    target.RemovePreTargetHandler(&destroyer);
  }
}

}  // namespace ui
