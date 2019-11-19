// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_EVENT_PROCESSOR_H_
#define UI_EVENTS_EVENT_PROCESSOR_H_

#include "base/memory/weak_ptr.h"
#include "ui/events/event_dispatcher.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_source.h"

namespace ui {

class EventTargeter;

// EventProcessor inherits EventSink to receive an event from an EventSource
// and dispatches it to a tree of EventTargets.
class EVENTS_EXPORT EventProcessor : public EventDispatcherDelegate,
                                     public EventSink {
 public:
  EventProcessor();
  ~EventProcessor() override;

  // EventSink overrides:
  EventDispatchDetails OnEventFromSource(Event* event) override;

  // Returns the EventTarget with the right EventTargeter that we should use for
  // dispatching this |event|.
  virtual EventTarget* GetRootForEvent(Event* event) = 0;

  // If the root target returned by GetRootForEvent() does not have a
  // targeter set, then the default targeter is used to find the target.
  virtual EventTargeter* GetDefaultEventTargeter() = 0;

 protected:
  // Invoked at the start of processing, before an EventTargeter is used to
  // find the target of the event. If processing should not take place, marks
  // |event| as handled. Otherwise updates |event| so that the targeter can
  // operate correctly (e.g., it can be used to update the location of the
  // event when dispatching from an EventSource in high-DPI) and updates any
  // members in the event processor as necessary.
  virtual void OnEventProcessingStarted(Event* event);

  // Invoked when the processing of |event| has finished (i.e., when no further
  // dispatching of |event| will be performed by this EventProcessor). Note
  // that the last target to which |event| was dispatched may have been
  // destroyed.
  virtual void OnEventProcessingFinished(Event* event);

 private:
  base::WeakPtrFactory<EventProcessor> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(EventProcessor);
};

}  // namespace ui

#endif  // UI_EVENTS_EVENT_PROCESSOR_H_
