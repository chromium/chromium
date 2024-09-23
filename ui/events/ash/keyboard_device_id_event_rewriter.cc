// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ash/keyboard_device_id_event_rewriter.h"

#include <memory>

#include "ui/events/ash/event_property.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_rewriter_continuation.h"

namespace ui {

KeyboardDeviceIdEventRewriter::KeyboardDeviceIdEventRewriter(
    KeyboardCapability* keyboard_capability)
    : keyboard_capability_(keyboard_capability) {}

KeyboardDeviceIdEventRewriter::~KeyboardDeviceIdEventRewriter() = default;

EventDispatchDetails KeyboardDeviceIdEventRewriter::RewriteEvent(
    const Event& event,
    const Continuation continuation) {
  std::optional<int> device_id = GetKeyboardDeviceIdInternal(event);
  if (!device_id.has_value()) {
    // No rewriting is needed.
    return continuation->SendEvent(&event);
  }

  // Remember last keyboard device id for following events.
  if (event.IsKeyEvent()) {
    last_keyboard_device_id_ = device_id.value();
  }

  if (event.source_device_id() == device_id) {
    // Rewritten device_id is the same as original one. Do nothing here and
    // apply following rewriters.
    return continuation->SendEvent(&event);
  }

  // The device id needs to be updated, so annotate as property.
  std::unique_ptr<Event> rewritten = event.Clone();
  SetKeyboardDeviceIdProperty(rewritten.get(), device_id.value());
  return continuation->SendEvent(rewritten.get());
}

// static
int KeyboardDeviceIdEventRewriter::GetKeyboardDeviceId(
    int keyboard_device_id,
    int last_keyboard_device_id,
    KeyboardCapability* keyboard_capability) {
  if (keyboard_device_id == ED_UNKNOWN_DEVICE) {
    return ED_UNKNOWN_DEVICE;
  }

  // Ignore virtual Xorg keyboard (magic that generates key repeat events).
  // Pretend that the previous real keyboard is the one that is still in use.
  if (keyboard_capability->GetDeviceType(keyboard_device_id) ==
      KeyboardCapability::DeviceType::kDeviceVirtualCoreKeyboard) {
    return last_keyboard_device_id;
  }

  return keyboard_device_id;
}

std::optional<int> KeyboardDeviceIdEventRewriter::GetKeyboardDeviceIdInternal(
    const Event& event) const {
  switch (event.type()) {
    case EventType::kKeyPressed:
    case EventType::kKeyReleased:
      return GetKeyboardDeviceId(event.source_device_id(),
                                 last_keyboard_device_id_,
                                 keyboard_capability_);
    case EventType::kMousePressed:
    case EventType::kMouseReleased:
    case EventType::kMousewheel:
    case EventType::kTouchPressed:
    case EventType::kTouchReleased:
      // Returns device_id for the last keyboard event for motion events.
      // This will be used for modifier flags rewriting in later stage.
      return last_keyboard_device_id_;

    default:
      return std::nullopt;
  }
}

}  // namespace ui
