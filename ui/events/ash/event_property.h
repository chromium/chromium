// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_ASH_EVENT_PROPERTY_H_
#define UI_EVENTS_ASH_EVENT_PROPERTY_H_

namespace ui {
class Event;

// Returns keyboard device_id for the `event`. If there's property set via
// SetKeyboardDeviceId, the value will be returned. Otherwise, `event`'s
// `source_device_id` will be returned, which assumes the event is
// KeyEvent.
int GetKeyboardDeviceIdProperty(const Event& event);

// Sets the device_id to the `event`'s property.
void SetKeyboardDeviceIdProperty(Event* event, int device_id);

}  // namespace ui

#endif  // UI_EVENTS_ASH_EVENT_PROPERTY_H_
