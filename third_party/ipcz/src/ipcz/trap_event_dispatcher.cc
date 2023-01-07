// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipcz/trap_event_dispatcher.h"

namespace ipcz {

TrapEventDispatcher::TrapEventDispatcher() = default;

TrapEventDispatcher::~TrapEventDispatcher() {
  DispatchAll();
}

void TrapEventDispatcher::DeferEvent(IpczTrapEventHandler handler,
                                     uintptr_t context,
                                     IpczTrapConditionFlags flags,
                                     const IpczPortalStatus& status) {
  events_.emplace_back(handler, context, flags, status);
}

void TrapEventDispatcher::DispatchAll() {
  for (const Event& event : events_) {
    const IpczTrapEvent trap_event = {
        .size = sizeof(trap_event),
        .context = event.context,
        .condition_flags = event.flags,
        .status = &event.status,
    };
    event.handler(&trap_event);
  }
}

TrapEventDispatcher::Event::Event() = default;

TrapEventDispatcher::Event::Event(IpczTrapEventHandler handler,
                                  uintptr_t context,
                                  IpczTrapConditionFlags flags,
                                  IpczPortalStatus status)
    : handler(handler), context(context), flags(flags), status(status) {}

TrapEventDispatcher::Event::Event(const Event&) = default;

TrapEventDispatcher::Event& TrapEventDispatcher::Event::operator=(
    const Event&) = default;

TrapEventDispatcher::Event::~Event() = default;

}  // namespace ipcz
