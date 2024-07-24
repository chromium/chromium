// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_rewriter.h"

#include <list>
#include <map>
#include <set>
#include <utility>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/types/cxx23_to_underlying.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/test_event_source.h"
#include "ui/events/test/test_event_target.h"

namespace ui {

namespace {

using test::TestEventTarget;

// TestEventRewriteSink is set up with a sequence of event types,
// and fails if the events received via OnEventFromSource() do not match
// this sequence. These expected event types are consumed on receipt.
class TestEventRewriteSink : public EventSink {
 public:
  explicit TestEventRewriteSink(EventTarget* expected_target)
      : expected_target_(expected_target) {}

  TestEventRewriteSink(const TestEventRewriteSink&) = delete;
  TestEventRewriteSink& operator=(const TestEventRewriteSink&) = delete;

  ~TestEventRewriteSink() override { CheckAllReceived(); }

  void AddExpectedEvent(EventType type) { expected_events_.push_back(type); }
  // Test that all expected events have been received.
  void CheckAllReceived() { EXPECT_TRUE(expected_events_.empty()); }

  // EventSink override:
  EventDispatchDetails OnEventFromSource(Event* event) override {
    EXPECT_FALSE(expected_events_.empty());
    EXPECT_EQ(expected_events_.front(), event->type());
    expected_events_.pop_front();
    EXPECT_EQ(expected_target_, event->target());
    return EventDispatchDetails();
  }

 private:
  std::list<EventType> expected_events_;
  const raw_ptr<EventTarget> expected_target_;
};

std::unique_ptr<Event> CreateEventForType(EventType type) {
  switch (type) {
    case EventType::kCancelMode:
      return std::make_unique<CancelModeEvent>();
    case EventType::kMouseDragged:
    case EventType::kMousePressed:
    case EventType::kMouseReleased:
      return std::make_unique<MouseEvent>(type, gfx::Point(), gfx::Point(),
                                          base::TimeTicks::Now(), 0, 0);
    case EventType::kKeyPressed:
    case EventType::kKeyReleased:
      return std::make_unique<KeyEvent>(type, ui::VKEY_TAB, DomCode::NONE, 0);
    case EventType::kScrollFlingCancel:
    case EventType::kScrollFlingStart:
      return std::make_unique<ScrollEvent>(
          type, gfx::Point(), base::TimeTicks::Now(), 0, 0, 0, 0, 0, 0);
    default:
      NOTREACHED_IN_MIGRATION() << base::to_underlying(type);
      return nullptr;
  }
}

class TestEventRewriteSource : public test::TestEventSource {
 public:
  TestEventRewriteSource(EventSink* sink, EventTarget* target)
      : TestEventSource(sink), target_(target) {}
  EventDispatchDetails Send(EventType type) {
    std::unique_ptr<Event> event = CreateEventForType(type);
    Event::DispatcherApi(event.get()).set_target(target_);
    return TestEventSource::Send(event.get());
  }

 private:
  const raw_ptr<EventTarget> target_;
};

// This EventRewriter always returns the same status, and if rewriting, the
// same event type; it is used to test simple rewriting, and rewriter addition,
// removal, and sequencing. Consequently EVENT_REWRITE_DISPATCH_ANOTHER is not
// supported here (calls to NextDispatchEvent() would continue indefinitely).
class TestConstantEventRewriterOld : public EventRewriter {
 public:
  TestConstantEventRewriterOld(EventRewriteStatus status, EventType type)
      : status_(status), type_(type) {
    CHECK_NE(EVENT_REWRITE_DISPATCH_ANOTHER, status);
  }

  EventRewriteStatus RewriteEvent(
      const Event& event,
      std::unique_ptr<Event>* rewritten_event) override {
    if (status_ == EVENT_REWRITE_REWRITTEN)
      *rewritten_event = CreateEventForType(type_);
    return status_;
  }
  EventRewriteStatus NextDispatchEvent(
      const Event& last_event,
      std::unique_ptr<Event>* new_event) override {
    NOTREACHED_IN_MIGRATION();
    return status_;
  }
  bool SupportsNonRootLocation() const override { return true; }

 private:
  EventRewriteStatus status_;
  EventType type_;
};

// This EventRewriter runs a simple state machine; it is used to test
// EVENT_REWRITE_DISPATCH_ANOTHER.
class TestStateMachineEventRewriterOld : public EventRewriter {
 public:
  TestStateMachineEventRewriterOld() = default;

  void AddRule(int from_state,
               EventType from_type,
               int to_state,
               EventType to_type,
               EventRewriteStatus to_status) {
    RewriteResult r = {to_state, to_type, to_status};
    rules_.emplace(RewriteCase(from_state, from_type), r);
  }
  EventRewriteStatus RewriteEvent(
      const Event& event,
      std::unique_ptr<Event>* rewritten_event) override {
    auto find = rules_.find(RewriteCase(state_, event.type()));
    if (find == rules_.end())
      return EVENT_REWRITE_CONTINUE;
    if ((find->second.status == EVENT_REWRITE_REWRITTEN) ||
        (find->second.status == EVENT_REWRITE_DISPATCH_ANOTHER)) {
      *rewritten_event = CreateEventForType(find->second.type);
      has_rewritten_event_ = true;
    } else {
      has_rewritten_event_ = false;
    }
    state_ = find->second.state;
    return find->second.status;
  }
  EventRewriteStatus NextDispatchEvent(
      const Event& last_event,
      std::unique_ptr<Event>* new_event) override {
    EXPECT_TRUE(has_rewritten_event_);
    EXPECT_FALSE(new_event->get() && new_event->get() == &last_event);
    return RewriteEvent(last_event, new_event);
  }
  bool SupportsNonRootLocation() const override { return true; }

 private:
  typedef std::pair<int, EventType> RewriteCase;
  struct RewriteResult {
    int state;
    EventType type;
    EventRewriteStatus status;
  };
  typedef std::map<RewriteCase, RewriteResult> RewriteRules;
  RewriteRules rules_;
  bool has_rewritten_event_ = false;
  int state_ = 0;
};

// This EventRewriter always accepts the original event. It is used to test
// simple rewriting, and rewriter addition, removal, and sequencing.
class TestAlwaysAcceptEventRewriter : public EventRewriter {
 public:
  TestAlwaysAcceptEventRewriter() {}
  EventDispatchDetails RewriteEvent(const Event& event,
                                    const Continuation continuation) override {
    return SendEvent(continuation, &event);
  }
};

// This EventRewriter always rewrites with the same event type; it is used
// to test simple rewriting, and rewriter addition, removal, and sequencing.
class TestConstantEventRewriter : public EventRewriter {
 public:
  explicit TestConstantEventRewriter(EventType type) : type_(type) {}
  EventDispatchDetails RewriteEvent(const Event& event,
                                    const Continuation continuation) override {
    std::unique_ptr<Event> replacement_event = CreateEventForType(type_);
    SetEventTarget(*replacement_event, event.target());
    return SendEventFinally(continuation, replacement_event.get());
  }

 private:
  EventType type_;
};

// This EventRewriter runs a simple state machine; it is used to test
// EVENT_REWRITE_DISPATCH_ANOTHER.
class TestStateMachineEventRewriter : public EventRewriter {
 public:
  enum RewriteAction { ACCEPT, DISCARD, REPLACE };
  enum StateAction { RETURN, PROCEED };
  TestStateMachineEventRewriter() : state_(0) {}
  void AddRule(int from_state,
               EventType from_type,
               int to_state,
               EventType to_type,
               RewriteAction rewrite_action,
               StateAction state_action) {
    RewriteResult r = {to_state, to_type, rewrite_action, state_action};
    rules_.insert({RewriteCase(from_state, from_type), r});
  }
  EventDispatchDetails RewriteEvent(const Event& event,
                                    const Continuation continuation) override {
    for (;;) {
      RewriteRules::iterator find =
          rules_.find(RewriteCase(state_, event.type()));
      if (find == rules_.end())
        return SendEvent(continuation, &event);
      state_ = find->second.state;
      EventDispatchDetails details;
      switch (find->second.rewrite_action) {
        case ACCEPT:
          details = SendEvent(continuation, &event);
          break;
        case DISCARD:
          break;
        case REPLACE:
          auto rewritten_event = CreateEventForType(find->second.type);
          SetEventTarget(*rewritten_event, event.target());
          details = SendEventFinally(continuation, rewritten_event.get());
          break;
      }
      if (details.dispatcher_destroyed || find->second.state_action == RETURN)
        return details;
    }
    NOTREACHED_IN_MIGRATION();
  }

 private:
  typedef std::pair<int, EventType> RewriteCase;
  struct RewriteResult {
    int state;
    EventType type;
    RewriteAction rewrite_action;
    StateAction state_action;
  };
  typedef std::map<RewriteCase, RewriteResult> RewriteRules;
  RewriteRules rules_;
  int state_;
};

}  // namespace

TEST(EventRewriterTest, EventRewritingOld) {
  // TestEventRewriter r0 always rewrites events to EventType::kCancelMode;
  // it is placed at the beginning of the chain and later removed,
  // to verify that rewriter removal works.
  TestConstantEventRewriterOld r0(EVENT_REWRITE_REWRITTEN,
                                  EventType::kCancelMode);

  // TestEventRewriter r1 always returns EVENT_REWRITE_CONTINUE;
  // it is at the beginning of the chain (once r0 is removed)
  // to verify that a later rewriter sees the events.
  TestConstantEventRewriterOld r1(EVENT_REWRITE_CONTINUE, EventType::kUnknown);

  // TestEventRewriter r2 has a state machine, primarily to test
  // |EVENT_REWRITE_DISPATCH_ANOTHER|.
  TestStateMachineEventRewriterOld r2;

  // TestEventRewriter r3 always rewrites events to EventType::kCancelMode;
  // it is placed at the end of the chain to verify that previously
  // rewritten events are not passed further down the chain.
  TestConstantEventRewriterOld r3(EVENT_REWRITE_REWRITTEN,
                                  EventType::kCancelMode);

  TestEventTarget t;
  TestEventRewriteSink p(&t);
  TestEventRewriteSource s(&p, &t);
  s.AddEventRewriter(&r0);
  s.AddEventRewriter(&r1);
  s.AddEventRewriter(&r2);

  // These events should be rewritten by r0 to EventType::kCancelMode.
  p.AddExpectedEvent(EventType::kCancelMode);
  s.Send(EventType::kMouseDragged);
  p.AddExpectedEvent(EventType::kCancelMode);
  s.Send(EventType::kMousePressed);
  p.CheckAllReceived();

  // Remove r0, and verify that it's gone and that events make it through.
  // - r0 is removed, so the resulting event should NOT be
  // EventType::kCancelMode.
  // - r2 should rewrite EventType::kScrollFlingStart to
  // EventType::kScrollFlingCancel,
  //   and skip subsequent rewriters, so the resulting event should be
  //   EventType::kScrollFlingCancel.
  // - r3 should be skipped after r2 returns, so the resulting event
  //   should NOT be EventType::kCancelMode.
  s.AddEventRewriter(&r3);
  s.RemoveEventRewriter(&r0);
  // clang-format off
  r2.AddRule(0, EventType::kScrollFlingStart,
             0, EventType::kScrollFlingCancel, EVENT_REWRITE_REWRITTEN);
  // clang-format on
  p.AddExpectedEvent(EventType::kScrollFlingCancel);
  s.Send(EventType::kScrollFlingStart);
  p.CheckAllReceived();
  s.RemoveEventRewriter(&r3);

  // Verify EVENT_REWRITE_DISPATCH_ANOTHER using a state machine
  // (that happens to be analogous to sticky keys).
  // clang-format off
  r2.AddRule(0, EventType::kKeyPressed,
             1, EventType::kKeyPressed, EVENT_REWRITE_CONTINUE);
  r2.AddRule(1, EventType::kMousePressed,
             0, EventType::kMousePressed, EVENT_REWRITE_CONTINUE);
  r2.AddRule(1, EventType::kKeyReleased,
             2, EventType::kKeyReleased, EVENT_REWRITE_DISCARD);
  r2.AddRule(2, EventType::kMouseReleased,
             3, EventType::kMouseReleased, EVENT_REWRITE_DISPATCH_ANOTHER);
  r2.AddRule(3, EventType::kMouseReleased,
             0, EventType::kKeyReleased, EVENT_REWRITE_REWRITTEN);
  // clang-format on
  p.AddExpectedEvent(EventType::kKeyPressed);
  s.Send(EventType::kKeyPressed);
  s.Send(EventType::kKeyReleased);
  p.AddExpectedEvent(EventType::kMousePressed);
  s.Send(EventType::kMousePressed);

  // Removing rewriter r1 shouldn't affect r2.
  s.RemoveEventRewriter(&r1);

  // Continue with the state-based rewriting.
  p.AddExpectedEvent(EventType::kMouseReleased);
  p.AddExpectedEvent(EventType::kKeyReleased);
  s.Send(EventType::kMouseReleased);
  p.CheckAllReceived();
}

TEST(EventRewriterTest, EventRewriting) {
  // TestEventRewriter r0 always rewrites events to EventType::kCancelMode;
  // it is placed at the beginning of the chain and later removed,
  // to verify that rewriter removal works.
  TestConstantEventRewriter r0(EventType::kCancelMode);

  // TestEventRewriter r1 always returns EVENT_REWRITE_CONTINUE;
  // it is at the beginning of the chain (once r0 is removed)
  // to verify that a later rewriter sees the events.
  TestAlwaysAcceptEventRewriter r1;

  // TestEventRewriter r2 has a state machine, primarily to test
  // |EVENT_REWRITE_DISPATCH_ANOTHER|.
  TestStateMachineEventRewriter r2;

  // TestEventRewriter r3 always rewrites events to EventType::kCancelMode;
  // it is placed at the end of the chain to verify that previously
  // rewritten events are not passed further down the chain.
  TestConstantEventRewriter r3(EventType::kCancelMode);

  TestEventTarget t;
  TestEventRewriteSink p(&t);
  TestEventRewriteSource s(&p, &t);
  s.AddEventRewriter(&r0);
  s.AddEventRewriter(&r1);
  s.AddEventRewriter(&r2);

  // These events should be rewritten by r0 to EventType::kCancelMode.
  p.AddExpectedEvent(EventType::kCancelMode);
  s.Send(EventType::kMouseDragged);
  p.AddExpectedEvent(EventType::kCancelMode);
  s.Send(EventType::kMousePressed);
  p.CheckAllReceived();

  // Remove r0, and verify that it's gone and that events make it through.
  // - r0 is removed, so the resulting event should NOT be
  // EventType::kCancelMode.
  // - r2 should rewrite EventType::kScrollFlingStart to
  // EventType::kScrollFlingCancel,
  //   and skip subsequent rewriters, so the resulting event should be
  //   EventType::kScrollFlingCancel.
  // - r3 should be skipped after r2 returns, so the resulting event
  //   should NOT be EventType::kCancelMode.
  s.AddEventRewriter(&r3);
  s.RemoveEventRewriter(&r0);
  r2.AddRule(0, EventType::kScrollFlingStart, 0, EventType::kScrollFlingCancel,
             TestStateMachineEventRewriter::REPLACE,
             TestStateMachineEventRewriter::RETURN);
  p.AddExpectedEvent(EventType::kScrollFlingCancel);
  s.Send(EventType::kScrollFlingStart);
  p.CheckAllReceived();
  s.RemoveEventRewriter(&r3);

  // Verify replacing an event with multiple events using a state machine
  // (that happens to be analogous to sticky keys).
  r2.AddRule(0, EventType::kKeyPressed, 1, EventType::kUnknown,
             TestStateMachineEventRewriter::ACCEPT,
             TestStateMachineEventRewriter::RETURN);
  r2.AddRule(1, EventType::kMousePressed, 0, EventType::kUnknown,
             TestStateMachineEventRewriter::ACCEPT,
             TestStateMachineEventRewriter::RETURN);
  r2.AddRule(1, EventType::kKeyReleased, 2, EventType::kUnknown,
             TestStateMachineEventRewriter::DISCARD,
             TestStateMachineEventRewriter::RETURN);
  r2.AddRule(2, EventType::kMouseReleased, 3, EventType::kMouseReleased,
             TestStateMachineEventRewriter::REPLACE,
             TestStateMachineEventRewriter::PROCEED);
  r2.AddRule(3, EventType::kMouseReleased, 0, EventType::kKeyReleased,
             TestStateMachineEventRewriter::REPLACE,
             TestStateMachineEventRewriter::RETURN);
  p.AddExpectedEvent(EventType::kKeyPressed);
  s.Send(EventType::kKeyPressed);  // state 0 EventType::kKeyPressed -> 1 ACCEPT
                                   // EventType::kKeyPressed
  s.Send(
      EventType::kKeyReleased);  // state 1 EventType::kKeyReleased -> 2 DISCARD
  p.AddExpectedEvent(EventType::kMousePressed);
  s.Send(EventType::kMousePressed);  // no matching rule; pass event through.

  // Removing rewriter r1 shouldn't affect r2.
  s.RemoveEventRewriter(&r1);

  // Continue with the state-based rewriting.
  p.AddExpectedEvent(EventType::kMouseReleased);
  p.AddExpectedEvent(EventType::kKeyReleased);
  s.Send(EventType::kMouseReleased);  // 2 EventType::kMouseReleased -> 3
                                      // PROCEED EventType::kMouseReleased 3
                                      // EventType::kMouseReleased -> 0 REPLACE
                                      // EventType::kKeyReleased
  p.CheckAllReceived();
}

}  // namespace ui
