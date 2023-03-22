// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event_targeter.h"

namespace ui {

EventTargeter::EventTargeter() = default;

EventTargeter::~EventTargeter() = default;

EventSink* EventTargeter::GetNewEventSinkForEvent(
    const EventTarget* current_root,
    EventTarget* target,
    Event* in_out_event) {
  return nullptr;
}

}  // namespace ui
