// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_processor.h"

#include "ui/events/event_target.h"
#include "ui/events/event_targeter.h"

namespace ui {

EventProcessor::EventProcessor() {}

EventProcessor::~EventProcessor() {}

EventDispatchDetails EventProcessor::OnEventFromSource(Event* event) {
  base::WeakPtr<EventProcessor> weak_this = weak_ptr_factory_.GetWeakPtr();
  // If |event| is in the process of being dispatched or has already been
  // dispatched, then dispatch a copy of the event instead. We expect event
  // target to be already set if event phase is after EP_PREDISPATCH.
  bool dispatch_original_event = event->phase() == EP_PREDISPATCH;
  DCHECK(dispatch_original_event || event->target());
  Event* event_to_dispatch = event;
  std::unique_ptr<Event> event_copy;
  if (!dispatch_original_event) {
    event_copy = event->Clone();
    event_to_dispatch = event_copy.get();
  }

  EventDispatchDetails details;
  OnEventProcessingStarted(event_to_dispatch);
  EventTarget* target = nullptr;
  if (!event_to_dispatch->handled()) {
    EventTarget* root = GetRootForEvent(event_to_dispatch);
    DCHECK(root);
    EventTargeter* targeter = root->GetEventTargeter();
    base::WeakPtr<EventTargeter> weak_targeter;
    if (targeter) {
      weak_targeter = targeter->GetWeakPtr();
      target = targeter->FindTargetForEvent(root, event_to_dispatch);
    } else {
      targeter = GetDefaultEventTargeter();
      weak_targeter = targeter->GetWeakPtr();
      if (event_to_dispatch->target())
        target = root;
      else
        target = targeter->FindTargetForEvent(root, event_to_dispatch);
    }
    DCHECK(targeter);
    // FindTargetForEvent may dispatch event, which may delete the event
    // processor or targeter.
    // TODO(crbug.com/40232309): Remove these returns and change this to DCHECK.
    if (!weak_this) {
      details.dispatcher_destroyed = true;
      return details;
    }
    if (!weak_targeter) {
      return details;
    }

    auto* new_event_sink =
        targeter->GetNewEventSinkForEvent(root, target, event_to_dispatch);

    if (new_event_sink) {
      std::ignore = new_event_sink->OnEventFromSource(event_to_dispatch);
      if (!weak_this) {
        details.dispatcher_destroyed = true;
        return details;
      }
    } else {
      while (target) {
        details = DispatchEvent(target, event_to_dispatch);

        if (!dispatch_original_event) {
          if (event_to_dispatch->stopped_propagation()) {
            event->StopPropagation();
          } else if (event_to_dispatch->handled()) {
            event->SetHandled();
          }
        }

        if (details.dispatcher_destroyed) {
          return details;
        }

        if (!weak_this) {
          details.dispatcher_destroyed = true;
          return details;
        }

        if (details.target_destroyed || event->handled() || !target ||
            !weak_targeter) {
          break;
        }

        DCHECK(targeter);
        target = targeter->FindNextBestTarget(target, event_to_dispatch);
      }
    }
  }
  OnEventProcessingFinished(event);
  return details;
}

void EventProcessor::OnEventProcessingStarted(Event* event) {
}

void EventProcessor::OnEventProcessingFinished(Event* event) {}

}  // namespace ui
