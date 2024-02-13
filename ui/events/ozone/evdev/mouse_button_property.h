// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_MOUSE_BUTTON_PROPERTY_H_
#define UI_EVENTS_OZONE_EVDEV_MOUSE_BUTTON_PROPERTY_H_

#include <cstdint>
#include <optional>

namespace ui {

class Event;

// Retrieves the Forward/Back Mouse button property from the given event.
// Returns nullopt if the property does not exist on the given event.
std::optional<uint32_t> GetForwardBackMouseButtonProperty(const Event& event);

// Sets the Forward/Back Mouse button property on the given event to be used to
// differentiate between the different forward/back buttons on devices.
void SetForwardBackMouseButtonProperty(Event& event, uint32_t button);

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_MOUSE_BUTTON_PROPERTY_H_
