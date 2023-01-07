// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_X11_EVENT_SENDER_H_
#define UI_AURA_TEST_X11_EVENT_SENDER_H_

#include "ui/aura/window_tree_host.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/x/connection.h"
#include "ui/gfx/x/xproto_util.h"

namespace aura {

namespace test {

// The root, time, root_x, and root_y fields of |xevent| may be modified.
template <typename T>
void PostEventToWindowTreeHost(WindowTreeHost* host, T* xevent) {
  auto* connection = x11::Connection::Get();
  x11::Window xwindow = static_cast<x11::Window>(host->GetAcceleratedWidget());
  xevent->event = xwindow;

  xevent->root = connection->default_root();
  xevent->time = x11::Time::CurrentTime;

  gfx::Point point(xevent->event_x, xevent->event_y);
  host->ConvertDIPToScreenInPixels(&point);
  xevent->root_x = point.x();
  xevent->root_y = point.y();

  x11::SendEvent(*xevent, xwindow, x11::EventMask::NoEvent);
  connection->Flush();
}

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_TEST_X11_EVENT_SENDER_H_
