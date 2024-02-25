// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/test/x11_event_waiter.h"

#include "base/task/single_thread_task_runner.h"
#include "ui/gfx/x/atom_cache.h"
#include "ui/gfx/x/xproto.h"

namespace ui {

// static
XEventWaiter* XEventWaiter::Create(x11::Window window,
                                   base::OnceClosure callback) {
  x11::ClientMessageEvent marker_event{
      .format = 8,
      .window = window,
      .type = MarkerEventAtom(),
  };

  x11::Connection::Get()->SendEvent(marker_event, window,
                                    x11::EventMask::NoEvent);
  x11::Connection::Get()->Flush();

  // Will be deallocated when the expected event is received.
  return new XEventWaiter(std::move(callback));
}

// XEventWaiter implementation
XEventWaiter::XEventWaiter(base::OnceClosure callback)
    : success_callback_(std::move(callback)) {
  x11::Connection::Get()->AddEventObserver(this);
}

XEventWaiter::~XEventWaiter() {
  x11::Connection::Get()->RemoveEventObserver(this);
}

void XEventWaiter::OnEvent(const x11::Event& xev) {
  auto* client = xev.As<x11::ClientMessageEvent>();
  if (client && client->type == MarkerEventAtom()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(success_callback_));
    delete this;
  }
}

// Returns atom that indidates that the XEvent is marker event.
x11::Atom XEventWaiter::MarkerEventAtom() {
  return x11::GetAtom("marker_event");
}

}  // namespace ui
