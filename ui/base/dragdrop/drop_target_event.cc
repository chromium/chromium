// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/dragdrop/drop_target_event.h"

#include "ui/events/event_utils.h"

namespace ui {

////////////////////////////////////////////////////////////////////////////////
// DropTargetEvent

DropTargetEvent::DropTargetEvent(const OSExchangeData& data,
                                 const gfx::PointF& location,
                                 const gfx::PointF& root_location,
                                 int source_operations)
    : LocatedEvent(EventType::kDropTargetEvent,
                   location,
                   root_location,
                   EventTimeForNow(),
                   0),
      data_(data),
      source_operations_(source_operations) {}

DropTargetEvent::DropTargetEvent(const DropTargetEvent& other)
    : LocatedEvent(other),
      data_(*other.data_),
      source_operations_(other.source_operations_) {}

std::unique_ptr<Event> DropTargetEvent::Clone() const {
  return std::make_unique<DropTargetEvent>(*this);
}

}  // namespace ui

