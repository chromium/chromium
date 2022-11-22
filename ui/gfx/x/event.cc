// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/x/event.h"

#include <xcb/xcb.h>

#include <cstring>

#include "base/check_op.h"
#include "base/memory/scoped_refptr.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto.h"
#include "ui/gfx/x/xproto_internal.h"
#include "ui/gfx/x/xproto_types.h"

namespace x11 {

Event::Event() = default;

Event::Event(scoped_refptr<base::RefCountedMemory> event_bytes,
             Connection* connection) {
  auto* xcb_event = reinterpret_cast<xcb_generic_event_t*>(
      const_cast<uint8_t*>(event_bytes->data()));
  uint8_t response_type = xcb_event->response_type & ~kSendEventMask;
  send_event_ = xcb_event->response_type & kSendEventMask;
  sequence_ = xcb_event->full_sequence;
  // KeymapNotify events are the only events that don't have a sequence.
  if (response_type != KeymapNotifyEvent::opcode) {
    // On the wire, events are 32 bytes except for generic events which are
    // trailed by additional data.  XCB inserts an extended 4-byte sequence
    // between the 32-byte event and the additional data, so we need to shift
    // the additional data over by 4 bytes so the event is back in its wire
    // format, which is what Xlib and XProto are expecting.
    if (response_type == GeGenericEvent::opcode) {
      auto* ge = reinterpret_cast<xcb_ge_event_t*>(xcb_event);
      constexpr size_t ge_length = sizeof(xcb_raw_generic_event_t);
      constexpr size_t offset = sizeof(ge->full_sequence);
      size_t extended_length = ge->length * 4;
      if (extended_length < ge_length) {
        // If the additional data is smaller than the fixed size event, shift
        // the additional data to the left.
        memmove(&ge->full_sequence, &ge[1], extended_length);
      } else {
        // Otherwise shift the fixed size event to the right.
        char* addr = reinterpret_cast<char*>(xcb_event);
        memmove(addr + offset, addr, ge_length);
        event_bytes = base::MakeRefCounted<OffsetRefCountedMemory>(
            event_bytes, offset, ge_length + extended_length);
        xcb_event = reinterpret_cast<xcb_generic_event_t*>(addr + offset);
      }
    }
  }

  // Xlib sometimes modifies |xcb_event|, so let it handle the event after
  // we parse it with ReadEvent().
  ReadBuffer buf(event_bytes);
  ReadEvent(this, connection, &buf);
}

Event::Event(Event&& event) {
  operator=(std::move(event));
}

Event& Event::operator=(Event&& event) {
  // `window_` borrowed from `event_`, so it must be reset first.
  window_ = std::move(event.window_);
  event_ = std::move(event.event_);
  type_id_ = event.type_id_;
  sequence_ = event.sequence_;
  send_event_ = event.send_event_;

  // Clear the old instance, to make sure an invalid state isn't going to be
  // used:
  event.type_id_ = 0;
  event.sequence_ = 0;
  event.send_event_ = false;
  return *this;
}

Event::~Event() = default;
}  // namespace x11
