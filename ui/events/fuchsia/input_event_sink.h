// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_FUCHSIA_INPUT_EVENT_SINK_H_
#define UI_EVENTS_FUCHSIA_INPUT_EVENT_SINK_H_

#include "ui/events/events_export.h"

namespace ui {

class Event;

// Translates Fuchsia input events to Chrome ui::Events.
class EVENTS_EXPORT InputEventSink {
 public:
  virtual void DispatchEvent(ui::Event* event) = 0;
};

}  // namespace ui

#endif  // UI_EVENTS_FUCHSIA_INPUT_EVENT_SINK_H_
