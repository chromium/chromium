// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_EVENT_REWRITER_H_
#define UI_EVENTS_EVENT_REWRITER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/events_export.h"
#include "ui/events/platform_event.h"

namespace ui {

class Event;
class EventRewriterContinuation;
class EventSource;  // TODO(kpschoedel): Remove with old API.

// Return status of EventRewriter operations; see that class below.
// TODO(kpschoedel): Remove old API.
enum EventRewriteStatus {
  // Nothing was done; no rewritten event returned. Pass the original
  // event to later rewriters, or send it to the EventSink if this
  // was the final rewriter.
  //
  // TODO(kpschoedel): Replace old API uses of
  //    return EVENT_REWRITE_CONTINUE;
  // with new API
  //    return SendEvent(continuation, &incoming_event);
  EVENT_REWRITE_CONTINUE,

  // The event has been rewritten. Send the rewritten event to the
  // EventSink instead of the original event (without sending
  // either to any later rewriters).
  //
  // TODO(kpschoedel): Replace old API uses of
  //    *rewritten_event = std::move(replacement_event);
  //    return EVENT_REWRITE_REWRITTEN;
  // with new API
  //    return SendEventFinally(continuation, replacement_event);
  EVENT_REWRITE_REWRITTEN,

  // The event should be discarded, neither passing it to any later
  // rewriters nor sending it to the EventSink.
  //
  // TODO(kpschoedel): Replace old API uses of
  //    return EVENT_REWRITE_DISCARD;
  // with new API
  //    return DiscardEvent(continuation);
  EVENT_REWRITE_DISCARD,

  // The event has been rewritten. As for EVENT_REWRITE_REWRITTEN,
  // send the rewritten event to the EventSink instead of the original
  // event (without sending either to any later rewriters).
  // In addition the rewriter has one or more additional new events
  // to be retrieved using |NextDispatchEvent()| and sent to the
  // EventSink.
  //
  // TODO(kpschoedel): Replace old API uses of
  //    *rewritten_event = std::move(new_event_1);
  //    // record new_event_2 … new_event_N
  //    return EVENT_REWRITE_DISPATCH_ANOTHER;
  // with new API
  //    details = SendEventFinally(new_event_1);
  //    ⋮
  //    return SendEventFinally(new_event_N);
  EVENT_REWRITE_DISPATCH_ANOTHER,
};

// EventRewriter provides a mechanism for Events to be rewritten
// before being dispatched from EventSource to EventSink.
class EVENTS_EXPORT EventRewriter {
 public:
  // Copyable opaque type, to be used only as an argument to SendEvent(),
  // SendEventFinally(), or DiscardEvent(). The current implementation is
  // a WeakPtr because some EventRewriters outlive their registration and
  // can try to send events through an absent source (e.g. from a timer).
  using Continuation = base::WeakPtr<EventRewriterContinuation>;

  EventRewriter() = default;

  EventRewriter(const EventRewriter&) = delete;
  EventRewriter& operator=(const EventRewriter&) = delete;

  virtual ~EventRewriter() = default;

  // Potentially rewrites (replaces) an event, possibly with multiple events,
  // or causes it to be discarded.
  //
  // To accept the supplied event without change,
  //    return SendEvent(continuation, &event)
  //
  // To replace the supplied event with a new event, call either
  //    return SendEvent(continuation, new_event)
  // or
  //    return SendEventFinally(continuation, new_event)
  // depending on whether or not |new_event| should be provided to any
  // later rewriters. These functions can be called more than once to
  // replace an incoming event with multiple new events; when doing so,
  // check |details.dispatcher_destroyed| after each call.
  //
  // To discard the incoming event without replacement,
  //    return DiscardEvent()
  //
  // In the common case of one event at a time, the EventDispatchDetails
  // from the above calls can and should be returned directly by RewriteEvent().
  // When a rewriter generates multiple events synchronously, it should
  // typically bail and return on a non-vacuous EventDispatchDetails.
  // When a rewriter generates events asynchronously (e.g. from a timer)
  // there is no opportunity to return the result directly, but a rewriter
  // can consider retaining it for the next call.
  //
  // The supplied WeakPtr<Continuation> can be saved in order to respond
  // asynchronously, e.g. after a double-click timeout. Normally, with
  // EventRewriters subordinate to EventSources, the Continuation lives as
  // long as the EventRewriter remains registered. If the continuation is not
  // valid, the Send functions will return with |details.dispatcher_destroyed|.
  //
  // Design note: We need to pass the continuation state explicitly because
  // Ash registers some EventRewriter instances with multiple EventSources.
  //
  virtual EventDispatchDetails RewriteEvent(const Event& event,
                                            const Continuation continuation);

  // Tells if this rewriter supports processing located events with location !=
  // root_location as well as honors event target when rewriting an event.
  // TODO(crbug.com/40274398): Remove once all rewriters honor event target.
  virtual bool SupportsNonRootLocation() const;

  // Potentially rewrites (replaces) an event, or requests it be discarded.
  // or discards an event. If the rewriter wants to rewrite an event, and
  // dispatch another event once the rewritten event is dispatched, it should
  // return EVENT_REWRITE_DISPATCH_ANOTHER, and return the next event to
  // dispatch from |NextDispatchEvent()|.
  // TODO(kpschoedel): Remove old API.
  virtual EventRewriteStatus RewriteEvent(
      const Event& event,
      std::unique_ptr<Event>* rewritten_event);

  // Supplies an additional event to be dispatched. It is only valid to
  // call this after the immediately previous call to |RewriteEvent()|
  // or |NextDispatchEvent()| has returned EVENT_REWRITE_DISPATCH_ANOTHER.
  // Should only return either EVENT_REWRITE_REWRITTEN or
  // EVENT_REWRITE_DISPATCH_ANOTHER; otherwise the previous call should not
  // have returned EVENT_REWRITE_DISPATCH_ANOTHER.
  // TODO(kpschoedel): Remove old API.
  virtual EventRewriteStatus NextDispatchEvent(
      const Event& last_event,
      std::unique_ptr<Event>* new_event);

 protected:
  // Forwards an event, through any subsequent rewriters.
  [[nodiscard]] static EventDispatchDetails SendEvent(
      const Continuation continuation,
      const Event* event);

  // Forwards an event, skipping any subsequent rewriters.
  [[nodiscard]] static EventDispatchDetails SendEventFinally(
      const Continuation continuation,
      const Event* event);

  // Discards an event, so that it will not be passed to the sink.
  [[nodiscard]] static EventDispatchDetails DiscardEvent(
      const Continuation continuation);

  // A helper that calls a protected EventSource function, which sends the event
  // to subsequent event rewriters on the source and onto its event sink.
  // TODO(kpschoedel): Replace with SendEvent(continuation, event).
  EventDispatchDetails SendEventToEventSource(EventSource* source,
                                              Event* event) const;

#if BUILDFLAG(IS_CHROMEOS)
  // Explicitly sets the `Event::native_event_` field bypassing any checks if
  // the `PlatformEvent` should be copied from one event to another. The
  // lifetime of `native_event` must be guaranteed to be longer than `event`. In
  // the context of event rewriting, this is almost always the case.
  void SetNativeEvent(Event& event, const PlatformEvent& native_event);
#endif

  void SetEventTarget(Event& event, EventTarget* target);
};

}  // namespace ui

#endif  // UI_EVENTS_EVENT_REWRITER_H_
