// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_desktop_window_move_client.h"

#include "ui/base/x/x11_util.h"
#include "ui/base/x/x11_window.h"
#include "ui/events/event.h"

namespace ui {

X11DesktopWindowMoveClient::X11DesktopWindowMoveClient(ui::XWindow* window)
    : window_(window) {}

X11DesktopWindowMoveClient::~X11DesktopWindowMoveClient() = default;

void X11DesktopWindowMoveClient::OnMouseMovement(const gfx::Point& screen_point,
                                                 int flags,
                                                 base::TimeTicks event_time) {
  gfx::Point system_loc = screen_point - window_offset_;
  window_->SetBounds(gfx::Rect(system_loc, window_->bounds().size()));
}

void X11DesktopWindowMoveClient::OnMouseReleased() {
  EndMoveLoop();
}

void X11DesktopWindowMoveClient::OnMoveLoopEnded() {}

bool X11DesktopWindowMoveClient::RunMoveLoop(bool can_grab_pointer,
                                             const gfx::Vector2d& drag_offset) {
  window_offset_ = drag_offset;
  return move_loop_.RunMoveLoop(can_grab_pointer, window_->last_cursor(),
                                window_->last_cursor());
}

void X11DesktopWindowMoveClient::EndMoveLoop() {
  move_loop_.EndMoveLoop();
}

}  // namespace ui
