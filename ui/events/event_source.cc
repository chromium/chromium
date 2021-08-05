// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_source.h"

#include "base/logging.h"
#include "ui/events/event_rewriter_continuation.h"
#include "ui/events/event_sink.h"

namespace ui {

namespace {

bool IsLocatedEventWithDifferentLocations(const Event& event) {
  if (!event.IsLocatedEvent())
    return false;
  const LocatedEvent* located_event = event.AsLocatedEvent();
  return located_event->target() &&
         located_event->location_f() != located_event->root_location_f();
}

}  // namespace

class EventSource::EventRewriterContinuationImpl
    : public EventRewriterContinuation {
 public:
  // Constructs an EventRewriterContinuationImpl at the end of the source's
  // rewriter list.
  static void Create(EventSource* const source, EventRewriter* rewriter) {
    DCHECK(source);
    DCHECK(rewriter);
    DCHECK(source->FindContinuation(rewriter) == source->rewriter_list_.end());
    source->rewriter_list_.push_back(
        std::make_unique<EventRewriterContinuationImpl>(source, rewriter));
    EventRewriterList::iterator it = source->rewriter_list_.end();
    --it;
    CHECK((*it)->rewriter() == rewriter);
    (*it)->self_ = it;
  }

  EventRewriterContinuationImpl(EventSource* const source,
                                EventRewriter* rewriter)
      : source_(source),
        rewriter_(rewriter),
        self_(source->rewriter_list_.end()) {}
  ~EventRewriterContinuationImpl() override {}

  EventRewriter* rewriter() const { return rewriter_; }

  base::WeakPtr<EventRewriterContinuationImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // EventRewriterContinuation overrides:
  EventDispatchDetails SendEvent(const Event* event) override {
    EventRewriterList::iterator next = self_;
    ++next;
    if (next == source_->rewriter_list_.end())
      return SendEventFinally(event);
    return (*next)->rewriter_->RewriteEvent(*event, (*next)->GetWeakPtr());
  }

  EventDispatchDetails SendEventFinally(const Event* event) override {
    return source_->DeliverEventToSink(const_cast<Event*>(event));
  }

  EventDispatchDetails DiscardEvent() override {
    ui::EventDispatchDetails details;
    details.event_discarded = true;
    return details;
  }

 private:
  EventSource* const source_;
  EventRewriter* rewriter_;
  EventRewriterList::iterator self_;

  base::WeakPtrFactory<EventRewriterContinuationImpl> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(EventRewriterContinuationImpl);
};

EventSource::EventSource() {}

EventSource::~EventSource() {}

void EventSource::AddEventRewriter(EventRewriter* rewriter) {
  EventRewriterContinuationImpl::Create(this, rewriter);
}

void EventSource::RemoveEventRewriter(EventRewriter* rewriter) {
  EventRewriterList::iterator it = FindContinuation(rewriter);
  if (it == rewriter_list_.end()) {
    // We need to tolerate attempting to remove an unregistered
    // EventRewriter, because many unit tests currently do so:
    // the rewriter gets added to the current root window source
    // on construction, and removed from the current root window
    // source on destruction, but the root window changes in
    // between.
    LOG(WARNING) << "EventRewriter not registered";
    return;
  }
  rewriter_list_.erase(it);
}

EventDispatchDetails EventSource::SendEventToSink(const Event* event) {
  return SendEventToSinkFromRewriter(event, nullptr);
}

EventDispatchDetails EventSource::DeliverEventToSink(Event* event) {
  EventSink* sink = GetEventSink();
  CHECK(sink);
  return sink->OnEventFromSource(event);
}

EventDispatchDetails EventSource::SendEventToSinkFromRewriter(
    const Event* event,
    const EventRewriter* rewriter) {
  std::unique_ptr<ui::Event> event_for_rewriting_ptr;
  const Event* event_for_rewriting = event;
  if (!rewriter_list_.empty() && IsLocatedEventWithDifferentLocations(*event)) {
    // EventRewriters don't expect an event with differing location and
    // root-location (because they don't honor the target). Provide such an
    // event.
    event_for_rewriting_ptr = ui::Event::Clone(*event);
    event_for_rewriting_ptr->AsLocatedEvent()->set_location_f(
        event_for_rewriting_ptr->AsLocatedEvent()->root_location_f());
    event_for_rewriting = event_for_rewriting_ptr.get();
  }
  EventRewriterList::iterator it = rewriter_list_.begin();
  if (rewriter) {
    // If a rewriter reposted |event|, only send it to subsequent rewriters.
    it = FindContinuation(rewriter);
    CHECK(it != rewriter_list_.end());
    ++it;
  }
  if (it == rewriter_list_.end())
    return DeliverEventToSink(const_cast<Event*>(event));
  return (*it)->rewriter()->RewriteEvent(*event_for_rewriting,
                                         (*it)->GetWeakPtr());
}

EventSource::EventRewriterList::iterator EventSource::FindContinuation(
    const EventRewriter* rewriter) {
  auto it = find_if(
      rewriter_list_.begin(), rewriter_list_.end(),
      [rewriter](const std::unique_ptr<EventRewriterContinuationImpl>& p)
          -> bool { return p->rewriter() == rewriter; });
  return it;
}

}  // namespace ui
