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

Event::Event(scoped_refptr<UnsizedRefCountedMemory> event_bytes,
             Connection* connection) {
  auto* xcb_event =
      reinterpret_cast<xcb_generic_event_t*>(event_bytes->bytes());
  uint8_t response_type = xcb_event->response_type & ~kSendEventMask;
  if (xcb_event->response_type & kSendEventMask) {
    send_event_ = true;
  }
  sequence_ = xcb_event->full_sequence;
  // On the wire, events are 32 bytes except for generic events which are
  // trailed by additional data.  XCB inserts an extended 4-byte sequence
  // between the 32-byte event and the additional data, so we need to shift
  // the additional data over by 4 bytes so the event is back in its wire
  // format, which is what XProto is expecting.
  if (response_type == GeGenericEvent::opcode) {
    auto* ge = reinterpret_cast<xcb_ge_event_t*>(xcb_event);
    const size_t extended_length = ge->length * 4;
    UNSAFE_TODO(memmove(&ge->full_sequence, &ge[1], extended_length));
  }
  connection->GetEventTypeAndOp(event_bytes->bytes(), &type_id_, &opcode_);
  if (type_id_) {
    raw_event_ = event_bytes;
  }
}

Event::Event(Event&& event) {
  operator=(std::move(event));
}

Event& Event::operator=(Event&& event) {
  send_event_ = event.send_event_;
  fabricated_ = event.fabricated_;
  type_id_ = event.type_id_;
  opcode_ = event.opcode_;
  sequence_ = event.sequence_;
  raw_event_ = std::move(event.raw_event_);
  event_ = std::move(event.event_);

  // Clear the old instance, to make sure it's in a valid state.
  event.send_event_ = false;
  event.fabricated_ = false;
  event.type_id_ = 0;
  event.opcode_ = 0;
  event.sequence_ = 0;
  return *this;
}

Event::~Event() = default;

void Event::Parse(void* event, Parser parser, Deleter deleter) {
  CHECK(type_id_);
  CHECK(!event_);
  CHECK(raw_event_);
  ReadBuffer read_buffer(raw_event_);
  parser(event, &read_buffer);
  event_ = {event, deleter};
  raw_event_.reset();
}

}  // namespace x11
