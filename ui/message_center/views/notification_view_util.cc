// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/event.h"
#include "ui/views/view.h"

namespace message_center {

namespace notification_view_util {
std::unique_ptr<ui::Event> ConvertToBoundedLocatedEvent(const ui::Event& event,
                                                        views::View* target) {
  // In case the animation is triggered from keyboard operation.
  if (!event.IsLocatedEvent() || !event.target())
    return nullptr;

  // Convert the point of |event| from the coordinate system of its target to
  // that of the passed in |target| and create a new LocatedEvent.
  std::unique_ptr<ui::Event> cloned_event = event.Clone();
  ui::LocatedEvent* located_event = cloned_event->AsLocatedEvent();
  event.target()->ConvertEventToTarget(target, located_event);

  // Use default animation if location is out of bounds.
  if (!target->HitTestPoint(located_event->location()))
    return nullptr;

  return cloned_event;
}

}  // namespace notification_view_util

}  // namespace message_center
