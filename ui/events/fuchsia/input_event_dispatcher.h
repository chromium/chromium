// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_FUCHSIA_INPUT_EVENT_DISPATCHER_H_
#define UI_EVENTS_FUCHSIA_INPUT_EVENT_DISPATCHER_H_

#include <fuchsia/ui/input/cpp/fidl.h>

#include "ui/events/events_export.h"

namespace ui {

class InputEventSink;

// Translates Fuchsia input events to Chrome ui::Events.
class EVENTS_EXPORT InputEventDispatcher {
 public:
  // |event_sink|: The recipient of any Chrome events that are processed from
  // Fuchsia events.
  explicit InputEventDispatcher(InputEventSink* event_sink);

  InputEventDispatcher(const InputEventDispatcher&) = delete;
  InputEventDispatcher& operator=(const InputEventDispatcher&) = delete;

  ~InputEventDispatcher();

  // Processes a Fuchsia |event| and dispatches Chrome ui::Events from it.
  // |event|'s coordinates must be specified in device-independent pixels.
  //
  // Returns true if the event was processed.
  bool ProcessEvent(const fuchsia::ui::input::InputEvent& event) const;

 private:
  bool ProcessMouseEvent(const fuchsia::ui::input::PointerEvent& event) const;
  bool ProcessTouchEvent(const fuchsia::ui::input::PointerEvent& event) const;

  InputEventSink* event_sink_;
};

}  // namespace ui

#endif  // UI_EVENTS_FUCHSIA_INPUT_EVENT_DISPATCHER_H_
