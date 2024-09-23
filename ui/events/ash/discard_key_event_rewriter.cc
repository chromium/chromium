// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ash/discard_key_event_rewriter.h"

#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/keycodes/dom/dom_code.h"

namespace ui {

DiscardKeyEventRewriter::DiscardKeyEventRewriter() = default;
DiscardKeyEventRewriter::~DiscardKeyEventRewriter() = default;

EventDispatchDetails DiscardKeyEventRewriter::RewriteEvent(
    const Event& event,
    const Continuation continuation) {
  std::unique_ptr<Event> rewritten_event;
  switch (event.type()) {
    case EventType::kKeyPressed:
    case EventType::kKeyReleased: {
      const KeyEvent& key_event = *event.AsKeyEvent();
      if (key_event.code() == DomCode::FN) {
        return DiscardEvent(continuation);
      }

      const int rewritten_flags = event.flags() & ~EF_FUNCTION_DOWN;
      if (event.flags() != rewritten_flags) {
        std::unique_ptr<KeyEvent> rewritten_key_event =
            std::make_unique<KeyEvent>(
                key_event.type(), key_event.key_code(), key_event.code(),
                rewritten_flags, key_event.GetDomKey(), key_event.time_stamp(),
                key_event.is_char());
        rewritten_key_event->set_scan_code(key_event.scan_code());
        rewritten_key_event->set_source_device_id(key_event.source_device_id());
        if (key_event.properties()) {
          rewritten_key_event->SetProperties(*key_event.properties());
        }
        rewritten_event = std::move(rewritten_key_event);
      }

      break;
    }
    default: {
      const int rewritten_flags = event.flags() & ~EF_FUNCTION_DOWN;
      if (event.flags() != rewritten_flags) {
        rewritten_event = event.Clone();

        // SetNativeEvent must be called explicitly as native events are not
        // copied on ChromeOS by default. This is because `PlatformEvent` is a
        // pointer by default, so its lifetime can not be guaranteed in general.
        // In this case, the lifetime of  `rewritten_event` is guaranteed to be
        // less than the original `event`.
        SetNativeEvent(*rewritten_event, event.native_event());

        rewritten_event->SetFlags(rewritten_flags);
      }
      break;
    }
  }

  return SendEvent(continuation,
                   rewritten_event ? rewritten_event.get() : &event);
}

}  // namespace ui
