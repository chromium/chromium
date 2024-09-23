// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_view_util.h"

#include "ui/events/event.h"
#include "ui/views/view.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ui/message_center/public/cpp/message_center_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace message_center::notification_view_util {

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

std::optional<size_t> GetLargeImageCornerRadius() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return kJellyImageCornerRadius;
#else
  return std::nullopt;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace message_center::notification_view_util
