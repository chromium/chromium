// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_EVENT_WITH_CALLBACK_H_
#define UI_EVENTS_BLINK_EVENT_WITH_CALLBACK_H_

#include <list>

#include "ui/events/blink/input_handler_proxy.h"
#include "ui/latency/latency_info.h"

namespace ui {

namespace test {
class InputHandlerProxyEventQueueTest;
}

class EventWithCallback {
 public:
  struct OriginalEventWithCallback {
    OriginalEventWithCallback(
        WebScopedInputEvent event,
        const LatencyInfo& latency,
        InputHandlerProxy::EventDispositionCallback callback);
    ~OriginalEventWithCallback();
    WebScopedInputEvent event_;
    LatencyInfo latency_;
    InputHandlerProxy::EventDispositionCallback callback_;
  };
  using OriginalEventList = std::list<OriginalEventWithCallback>;

  EventWithCallback(WebScopedInputEvent event,
                    const LatencyInfo& latency,
                    base::TimeTicks timestamp_now,
                    InputHandlerProxy::EventDispositionCallback callback);
  EventWithCallback(WebScopedInputEvent event,
                    const LatencyInfo& latency,
                    base::TimeTicks creation_timestamp,
                    base::TimeTicks last_coalesced_timestamp,
                    std::unique_ptr<OriginalEventList> original_events);
  ~EventWithCallback();

  bool CanCoalesceWith(const EventWithCallback& other) const WARN_UNUSED_RESULT;
  void CoalesceWith(EventWithCallback* other, base::TimeTicks timestamp_now);

  void RunCallbacks(InputHandlerProxy::EventDisposition,
                    const LatencyInfo& latency,
                    std::unique_ptr<DidOverscrollParams>);

  const blink::WebInputEvent& event() const { return *event_; }
  blink::WebInputEvent* event_pointer() { return event_.get(); }
  const LatencyInfo& latency_info() const { return latency_; }
  LatencyInfo* mutable_latency_info() { return &latency_; }
  base::TimeTicks creation_timestamp() const { return creation_timestamp_; }
  base::TimeTicks last_coalesced_timestamp() const {
    return last_coalesced_timestamp_;
  }
  size_t coalesced_count() const { return original_events_.size(); }
  OriginalEventList& original_events() { return original_events_; }
  // |first_original_event()| is used as ID for tracing.
  blink::WebInputEvent* first_original_event() {
    return original_events_.empty() ? nullptr
                                    : original_events_.front().event_.get();
  }

 private:
  friend class test::InputHandlerProxyEventQueueTest;

  void SetTickClockForTesting(std::unique_ptr<base::TickClock> tick_clock);

  WebScopedInputEvent event_;
  LatencyInfo latency_;
  OriginalEventList original_events_;

  base::TimeTicks creation_timestamp_;
  base::TimeTicks last_coalesced_timestamp_;
};

}  // namespace ui

#endif  // UI_EVENTS_BLINK_EVENT_WITH_CALLBACK_H_
