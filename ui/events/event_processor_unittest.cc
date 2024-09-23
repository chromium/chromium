// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <tuple>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_target_iterator.h"
#include "ui/events/event_targeter.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/events/test/test_event_handler.h"
#include "ui/events/test/test_event_processor.h"
#include "ui/events/test/test_event_target.h"
#include "ui/events/test/test_event_targeter.h"

typedef std::vector<std::string> HandlerSequenceRecorder;

namespace ui {
namespace test {

class EventProcessorTest : public testing::Test {
 public:
  EventProcessorTest() {}

  EventProcessorTest(const EventProcessorTest&) = delete;
  EventProcessorTest& operator=(const EventProcessorTest&) = delete;

  ~EventProcessorTest() override {}

 protected:
  // testing::Test:
  void SetUp() override {
    processor_.SetRoot(std::make_unique<TestEventTarget>());
    processor_.Reset();
    root()->SetEventTargeter(
        std::make_unique<TestEventTargeter>(root(), false));
  }

  TestEventTarget* root() {
    return static_cast<TestEventTarget*>(processor_.GetRoot());
  }

  TestEventProcessor* processor() {
    return &processor_;
  }

  void DispatchEvent(Event* event) {
    processor_.OnEventFromSource(event);
  }

  void SetTarget(TestEventTarget* target) {
    static_cast<TestEventTargeter*>(root()->GetEventTargeter())
        ->set_target(target);
  }

 private:
  TestEventProcessor processor_;
};

TEST_F(EventProcessorTest, Basic) {
  auto child = std::make_unique<TestEventTarget>();
  child->SetEventTargeter(
      std::make_unique<TestEventTargeter>(child.get(), false));
  SetTarget(child.get());
  root()->AddChild(std::move(child));

  MouseEvent mouse(EventType::kMouseMoved, gfx::Point(10, 10),
                   gfx::Point(10, 10), EventTimeForNow(), EF_NONE, EF_NONE);
  DispatchEvent(&mouse);
  EXPECT_TRUE(root()->child_at(0)->DidReceiveEvent(EventType::kMouseMoved));
  EXPECT_FALSE(root()->DidReceiveEvent(EventType::kMouseMoved));

  SetTarget(root());
  root()->RemoveChild(root()->child_at(0));
  DispatchEvent(&mouse);
  EXPECT_TRUE(root()->DidReceiveEvent(EventType::kMouseMoved));
}

// ReDispatchEventHandler is used to receive mouse events and forward them
// to a specified EventProcessor. Verifies that the event has the correct
// target and phase both before and after the nested event processing. Also
// verifies that the location of the event remains the same after it has
// been processed by the second EventProcessor.
class ReDispatchEventHandler : public TestEventHandler {
 public:
  ReDispatchEventHandler(EventProcessor* processor, EventTarget* target)
      : processor_(processor), expected_target_(target) {}

  ReDispatchEventHandler(const ReDispatchEventHandler&) = delete;
  ReDispatchEventHandler& operator=(const ReDispatchEventHandler&) = delete;

  ~ReDispatchEventHandler() override {}

  // TestEventHandler:
  void OnMouseEvent(MouseEvent* event) override {
    TestEventHandler::OnMouseEvent(event);

    EXPECT_EQ(expected_target_, event->target());
    EXPECT_EQ(EP_TARGET, event->phase());

    gfx::Point location(event->location());
    EventDispatchDetails details = processor_->OnEventFromSource(event);
    EXPECT_FALSE(details.dispatcher_destroyed);
    EXPECT_FALSE(details.target_destroyed);

    // The nested event-processing should not have mutated the target,
    // phase, or location of |event|.
    EXPECT_EQ(expected_target_, event->target());
    EXPECT_EQ(EP_TARGET, event->phase());
    EXPECT_EQ(location, event->location());
  }

 private:
  raw_ptr<EventProcessor> processor_;
  raw_ptr<EventTarget> expected_target_;
};

// Verifies that the phase and target information of an event is not mutated
// as a result of sending the event to an event processor while it is still
// being processed by another event processor.
TEST_F(EventProcessorTest, NestedEventProcessing) {
  // Add one child to the default event processor used in this test suite.
  auto child = std::make_unique<TestEventTarget>();
  SetTarget(child.get());
  root()->AddChild(std::move(child));

  // Define a second root target and child.
  auto second_root_scoped = std::make_unique<TestEventTarget>();
  TestEventTarget* second_root = second_root_scoped.get();
  auto second_child = std::make_unique<TestEventTarget>();
  second_root->SetEventTargeter(
      std::make_unique<TestEventTargeter>(second_child.get(), false));
  second_root->AddChild(std::move(second_child));

  // Define a second event processor which owns the second root.
  auto second_processor = std::make_unique<TestEventProcessor>();
  second_processor->SetRoot(std::move(second_root_scoped));

  // Indicate that an event which is dispatched to the child target owned by the
  // first event processor should be handled by |target_handler| instead.
  auto target_handler = std::make_unique<ReDispatchEventHandler>(
      second_processor.get(), root()->child_at(0));
  EventHandler* old_handler =
      root()->child_at(0)->SetTargetHandler(target_handler.get());

  // Dispatch a mouse event to the tree of event targets owned by the first
  // event processor, checking in ReDispatchEventHandler that the phase and
  // target information of the event is correct.
  MouseEvent mouse(EventType::kMouseMoved, gfx::Point(10, 10),
                   gfx::Point(10, 10), EventTimeForNow(), EF_NONE, EF_NONE);
  DispatchEvent(&mouse);

  // Verify also that |mouse| was seen by the child nodes contained in both
  // event processors and that the event was not handled.
  EXPECT_EQ(1, target_handler->num_mouse_events());
  EXPECT_TRUE(
      second_root->child_at(0)->DidReceiveEvent(EventType::kMouseMoved));
  EXPECT_FALSE(mouse.handled());
  second_root->child_at(0)->ResetReceivedEvents();
  root()->child_at(0)->ResetReceivedEvents();

  target_handler->Reset();

  // Indicate that the child of the second root should handle events, and
  // dispatch another mouse event to verify that it is marked as handled.
  second_root->child_at(0)->set_mark_events_as_handled(true);
  MouseEvent mouse2(EventType::kMouseMoved, gfx::Point(10, 10),
                    gfx::Point(10, 10), EventTimeForNow(), EF_NONE, EF_NONE);
  DispatchEvent(&mouse2);
  EXPECT_EQ(1, target_handler->num_mouse_events());
  EXPECT_TRUE(
      second_root->child_at(0)->DidReceiveEvent(EventType::kMouseMoved));
  EXPECT_TRUE(mouse2.handled());

  old_handler = root()->child_at(0)->SetTargetHandler(old_handler);
  EXPECT_EQ(old_handler, target_handler.get());
}

// Verifies that OnEventProcessingFinished() is called when an event
// has been handled.
TEST_F(EventProcessorTest, OnEventProcessingFinished) {
  auto child = std::make_unique<TestEventTarget>();
  child->set_mark_events_as_handled(true);
  SetTarget(child.get());
  root()->AddChild(std::move(child));

  // Dispatch a mouse event. We expect the event to be seen by the target,
  // handled, and we expect OnEventProcessingFinished() to be invoked once.
  MouseEvent mouse(EventType::kMouseMoved, gfx::Point(10, 10),
                   gfx::Point(10, 10), EventTimeForNow(), EF_NONE, EF_NONE);
  DispatchEvent(&mouse);
  EXPECT_TRUE(root()->child_at(0)->DidReceiveEvent(EventType::kMouseMoved));
  EXPECT_FALSE(root()->DidReceiveEvent(EventType::kMouseMoved));
  EXPECT_TRUE(mouse.handled());
  EXPECT_EQ(1, processor()->num_times_processing_finished());
}

// Verifies that OnEventProcessingStarted() has been called when starting to
// process an event, and that processing does not take place if
// OnEventProcessingStarted() marks the event as handled. Also verifies that
// OnEventProcessingFinished() is also called in either case.
TEST_F(EventProcessorTest, OnEventProcessingStarted) {
  auto child = std::make_unique<TestEventTarget>();
  SetTarget(child.get());
  root()->AddChild(std::move(child));

  // Dispatch a mouse event. We expect the event to be seen by the target,
  // OnEventProcessingStarted() should be called once, and
  // OnEventProcessingFinished() should be called once. The event should
  // remain unhandled.
  MouseEvent mouse(EventType::kMouseMoved, gfx::Point(10, 10),
                   gfx::Point(10, 10), EventTimeForNow(), EF_NONE, EF_NONE);
  DispatchEvent(&mouse);
  EXPECT_TRUE(root()->child_at(0)->DidReceiveEvent(EventType::kMouseMoved));
  EXPECT_FALSE(root()->DidReceiveEvent(EventType::kMouseMoved));
  EXPECT_FALSE(mouse.handled());
  EXPECT_EQ(1, processor()->num_times_processing_started());
  EXPECT_EQ(1, processor()->num_times_processing_finished());
  processor()->Reset();
  root()->ResetReceivedEvents();
  root()->child_at(0)->ResetReceivedEvents();

  // Dispatch another mouse event, but with OnEventProcessingStarted() marking
  // the event as handled to prevent processing. We expect the event to not be
  // seen by the target this time, but OnEventProcessingStarted() and
  // OnEventProcessingFinished() should both still be called once.
  processor()->set_should_processing_occur(false);
  MouseEvent mouse2(EventType::kMouseMoved, gfx::Point(10, 10),
                    gfx::Point(10, 10), EventTimeForNow(), EF_NONE, EF_NONE);
  DispatchEvent(&mouse2);
  EXPECT_FALSE(root()->child_at(0)->DidReceiveEvent(EventType::kMouseMoved));
  EXPECT_FALSE(root()->DidReceiveEvent(EventType::kMouseMoved));
  EXPECT_TRUE(mouse2.handled());
  EXPECT_EQ(1, processor()->num_times_processing_started());
  EXPECT_EQ(1, processor()->num_times_processing_finished());
}

// Tests that unhandled events are correctly dispatched to the next-best
// target as decided by the TestEventTargeter.
TEST_F(EventProcessorTest, DispatchToNextBestTarget) {
  auto child = std::make_unique<TestEventTarget>();
  auto grandchild = std::make_unique<TestEventTarget>();

  // Install a TestEventTargeter which permits bubbling.
  root()->SetEventTargeter(
      std::make_unique<TestEventTargeter>(grandchild.get(), true));
  child->AddChild(std::move(grandchild));
  root()->AddChild(std::move(child));

  ASSERT_EQ(1u, root()->child_count());
  ASSERT_EQ(1u, root()->child_at(0)->child_count());
  ASSERT_EQ(0u, root()->child_at(0)->child_at(0)->child_count());

  TestEventTarget* child_r = root()->child_at(0);
  TestEventTarget* grandchild_r = child_r->child_at(0);

  // When the root has a TestEventTargeter installed which permits bubbling,
  // events targeted at the grandchild target should be dispatched to all three
  // targets.
  KeyEvent key_event(EventType::kKeyPressed, VKEY_ESCAPE, EF_NONE);
  DispatchEvent(&key_event);
  EXPECT_TRUE(root()->DidReceiveEvent(EventType::kKeyPressed));
  EXPECT_TRUE(child_r->DidReceiveEvent(EventType::kKeyPressed));
  EXPECT_TRUE(grandchild_r->DidReceiveEvent(EventType::kKeyPressed));
  root()->ResetReceivedEvents();
  child_r->ResetReceivedEvents();
  grandchild_r->ResetReceivedEvents();

  // Add a pre-target handler on the child of the root that will mark the event
  // as handled. No targets in the hierarchy should receive the event.
  TestEventHandler handler;
  child_r->AddPreTargetHandler(&handler);
  key_event = KeyEvent(EventType::kKeyPressed, VKEY_ESCAPE, EF_NONE);
  DispatchEvent(&key_event);
  EXPECT_FALSE(root()->DidReceiveEvent(EventType::kKeyPressed));
  EXPECT_FALSE(child_r->DidReceiveEvent(EventType::kKeyPressed));
  EXPECT_FALSE(grandchild_r->DidReceiveEvent(EventType::kKeyPressed));
  EXPECT_EQ(1, handler.num_key_events());
  handler.Reset();

  // Add a post-target handler on the child of the root that will mark the event
  // as handled. Only the grandchild (the initial target) should receive the
  // event.
  child_r->RemovePreTargetHandler(&handler);
  child_r->AddPostTargetHandler(&handler);
  key_event = KeyEvent(EventType::kKeyPressed, VKEY_ESCAPE, EF_NONE);
  DispatchEvent(&key_event);
  EXPECT_FALSE(root()->DidReceiveEvent(EventType::kKeyPressed));
  EXPECT_FALSE(child_r->DidReceiveEvent(EventType::kKeyPressed));
  EXPECT_TRUE(grandchild_r->DidReceiveEvent(EventType::kKeyPressed));
  EXPECT_EQ(1, handler.num_key_events());
  handler.Reset();
  grandchild_r->ResetReceivedEvents();
  child_r->RemovePostTargetHandler(&handler);

  // Mark the event as handled when it reaches the EP_TARGET phase of
  // dispatch at the child of the root. The child and grandchild
  // targets should both receive the event, but the root should not.
  child_r->set_mark_events_as_handled(true);
  key_event = KeyEvent(EventType::kKeyPressed, VKEY_ESCAPE, EF_NONE);
  DispatchEvent(&key_event);
  EXPECT_FALSE(root()->DidReceiveEvent(EventType::kKeyPressed));
  EXPECT_TRUE(child_r->DidReceiveEvent(EventType::kKeyPressed));
  EXPECT_TRUE(grandchild_r->DidReceiveEvent(EventType::kKeyPressed));
  root()->ResetReceivedEvents();
  child_r->ResetReceivedEvents();
  grandchild_r->ResetReceivedEvents();
  child_r->set_mark_events_as_handled(false);
}

// Tests that unhandled events are seen by the correct sequence of
// targets, pre-target handlers, and post-target handlers when
// a TestEventTargeter is installed on the root target which permits bubbling.
TEST_F(EventProcessorTest, HandlerSequence) {
  auto child = std::make_unique<TestEventTarget>();
  auto grandchild = std::make_unique<TestEventTarget>();

  // Install a TestEventTargeter which permits bubbling.
  root()->SetEventTargeter(
      std::make_unique<TestEventTargeter>(grandchild.get(), true));
  child->AddChild(std::move(grandchild));
  root()->AddChild(std::move(child));

  ASSERT_EQ(1u, root()->child_count());
  ASSERT_EQ(1u, root()->child_at(0)->child_count());
  ASSERT_EQ(0u, root()->child_at(0)->child_at(0)->child_count());

  TestEventTarget* child_r = root()->child_at(0);
  TestEventTarget* grandchild_r = child_r->child_at(0);

  HandlerSequenceRecorder recorder;
  root()->set_target_name("R");
  root()->set_recorder(&recorder);
  child_r->set_target_name("C");
  child_r->set_recorder(&recorder);
  grandchild_r->set_target_name("G");
  grandchild_r->set_recorder(&recorder);

  TestEventHandler pre_root;
  pre_root.set_handler_name("PreR");
  pre_root.set_recorder(&recorder);
  root()->AddPreTargetHandler(&pre_root);

  TestEventHandler pre_child;
  pre_child.set_handler_name("PreC");
  pre_child.set_recorder(&recorder);
  child_r->AddPreTargetHandler(&pre_child);

  TestEventHandler pre_grandchild;
  pre_grandchild.set_handler_name("PreG");
  pre_grandchild.set_recorder(&recorder);
  grandchild_r->AddPreTargetHandler(&pre_grandchild);

  TestEventHandler post_root;
  post_root.set_handler_name("PostR");
  post_root.set_recorder(&recorder);
  root()->AddPostTargetHandler(&post_root);

  TestEventHandler post_child;
  post_child.set_handler_name("PostC");
  post_child.set_recorder(&recorder);
  child_r->AddPostTargetHandler(&post_child);

  TestEventHandler post_grandchild;
  post_grandchild.set_handler_name("PostG");
  post_grandchild.set_recorder(&recorder);
  grandchild_r->AddPostTargetHandler(&post_grandchild);

  MouseEvent mouse(EventType::kMouseMoved, gfx::Point(10, 10),
                   gfx::Point(10, 10), EventTimeForNow(), EF_NONE, EF_NONE);
  DispatchEvent(&mouse);

  std::string expected[] = { "PreR", "PreC", "PreG", "G", "PostG", "PostC",
      "PostR", "PreR", "PreC", "C", "PostC", "PostR", "PreR", "R", "PostR" };
  EXPECT_EQ(std::vector<std::string>(expected, expected + std::size(expected)),
            recorder);

  grandchild_r->RemovePreTargetHandler(&pre_grandchild);
  child_r->RemovePreTargetHandler(&pre_child);
  root()->RemovePreTargetHandler(&pre_root);

  grandchild_r->set_recorder(nullptr);
  child_r->set_recorder(nullptr);
  root()->set_recorder(nullptr);
}

namespace {

enum DestroyTarget { kProcessor, kTargeter };

class DestroyDuringDispatchEventProcessor : public TestEventProcessor {
 public:
  DestroyDuringDispatchEventProcessor() = default;
  DestroyDuringDispatchEventProcessor(
      const DestroyDuringDispatchEventProcessor&) = delete;
  DestroyDuringDispatchEventProcessor& operator=(
      const DestroyDuringDispatchEventProcessor&) = delete;
  ~DestroyDuringDispatchEventProcessor() override = default;

 protected:
  EventDispatchDetails PostDispatchEvent(EventTarget* target,
                                         const Event& event) override;
};

class DestroyDuringDispatchEventTarget : public TestEventTarget {
 public:
  explicit DestroyDuringDispatchEventTarget(DestroyTarget target)
      : destroy_target_(target),
        processor_(std::make_unique<DestroyDuringDispatchEventProcessor>()) {}

  DestroyDuringDispatchEventTarget(const DestroyDuringDispatchEventTarget&) =
      delete;
  DestroyDuringDispatchEventTarget& operator=(
      const DestroyDuringDispatchEventTarget&) = delete;

  TestEventProcessor* processor() { return processor_.get(); }

  void Destroy() {
    switch (destroy_target_) {
      case kProcessor:
        processor_.reset();
        break;
      case kTargeter:
        SetEventTargeter(nullptr);
    }
  }

 private:
  DestroyTarget destroy_target_;
  std::unique_ptr<TestEventProcessor> processor_;
};

EventDispatchDetails DestroyDuringDispatchEventProcessor::PostDispatchEvent(
    EventTarget* target,
    const Event& event) {
  static_cast<DestroyDuringDispatchEventTarget*>(target)->Destroy();
  return EventDispatchDetails();
}

}  // namespace

TEST(EventProcessorCrashTest, DestroyDuringDispatch) {
  for (auto destroy_target : {kProcessor, kTargeter}) {
    SCOPED_TRACE(destroy_target == kProcessor ? "Processor" : "Targeter");
    auto root = std::make_unique<TestEventTarget>();
    auto target =
        std::make_unique<DestroyDuringDispatchEventTarget>(destroy_target);
    root->SetEventTargeter(
        std::make_unique<TestEventTargeter>(target.get(), false));
    TestEventProcessor* processor = target->processor();
    auto* target_ptr = target.get();
    processor->SetRoot(std::move(root));

    MouseEvent mouse(EventType::kMouseMoved, gfx::Point(10, 10),
                     gfx::Point(10, 10), EventTimeForNow(), EF_NONE, EF_NONE);

    if (destroy_target == kProcessor) {
      EXPECT_TRUE(processor->OnEventFromSource(&mouse).dispatcher_destroyed);
    } else {
      EXPECT_FALSE(processor->OnEventFromSource(&mouse).dispatcher_destroyed);
      EXPECT_FALSE(target_ptr->GetEventTargeter());
    }
  }
}

namespace {

class DestroyDuringFindTargetEventTargeter : public TestEventTargeter {
 public:
  DestroyDuringFindTargetEventTargeter(std::unique_ptr<TestEventTarget> root,
                                       DestroyTarget target)
      : TestEventTargeter(nullptr, false),
        destroy_target_(target),
        root_(root.get()),
        processor_(std::make_unique<TestEventProcessor>()) {
    processor_->SetRoot(std::move(root));
  }
  DestroyDuringFindTargetEventTargeter(
      const DestroyDuringFindTargetEventTargeter&) = delete;
  DestroyDuringFindTargetEventTargeter& operator=(
      const DestroyDuringFindTargetEventTargeter&) = delete;
  ~DestroyDuringFindTargetEventTargeter() override = default;

  // EventTargeter:
  EventTarget* FindTargetForEvent(EventTarget* root, Event* event) override {
    switch (destroy_target_) {
      case kProcessor:
        processor_.reset();
        break;
      case kTargeter:
        processor_.release();
        DCHECK_EQ(this, root_->GetEventTargeter());
        root_->SetEventTargeter(nullptr);
    }
    return nullptr;
  }

  EventProcessor* processor() { return processor_.get(); }

 private:
  DestroyTarget destroy_target_;
  raw_ptr<TestEventTarget> root_;
  std::unique_ptr<TestEventProcessor> processor_;
};

}  // namespace

TEST(EventProcessorCrashTest, DestroyDuringFindTarget) {
  for (auto destroy_target : {kProcessor, kTargeter}) {
    SCOPED_TRACE(destroy_target == kProcessor ? "Processor" : "Targeter");
    auto root = std::make_unique<TestEventTarget>();
    TestEventTarget* root_ptr = root.get();
    auto event_targeter =
        std::make_unique<DestroyDuringFindTargetEventTargeter>(std::move(root),
                                                               destroy_target);
    auto* processor = event_targeter->processor();
    root_ptr->SetEventTargeter(std::move(event_targeter));

    MouseEvent mouse(EventType::kMouseMoved, gfx::Point(10, 10),
                     gfx::Point(10, 10), EventTimeForNow(), EF_NONE, EF_NONE);
    if (destroy_target == kProcessor) {
      EXPECT_TRUE(processor->OnEventFromSource(&mouse).dispatcher_destroyed);
    } else {
      EXPECT_FALSE(processor->OnEventFromSource(&mouse).dispatcher_destroyed);
      EXPECT_FALSE(root_ptr->GetEventTargeter());
      // TestEventTargeter releases the processor when deleting the targeter.
      delete processor;
    }
  }
}

}  // namespace test
}  // namespace ui
