// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/x11_desktop_window_move_client.h"

#include "base/functional/callback_helpers.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {

X11DesktopWindowMoveClient::Delegate::~Delegate() = default;

X11DesktopWindowMoveClient::X11DesktopWindowMoveClient(Delegate* window)
    : window_(window) {}

X11DesktopWindowMoveClient::~X11DesktopWindowMoveClient() = default;

void X11DesktopWindowMoveClient::OnMouseMovement(const gfx::Point& screen_point,
                                                 int flags,
                                                 base::TimeTicks event_time) {
  gfx::Point system_loc = screen_point - window_offset_;
  window_->SetBoundsOnMove(gfx::Rect(system_loc, window_->GetSize()));
}

void X11DesktopWindowMoveClient::OnMouseReleased() {
  EndMoveLoop();
}

void X11DesktopWindowMoveClient::OnMoveLoopEnded() {}

bool X11DesktopWindowMoveClient::RunMoveLoop(bool can_grab_pointer,
                                             const gfx::Vector2d& drag_offset) {
  window_offset_ = drag_offset;
  return move_loop_.RunMoveLoop(can_grab_pointer, window_->GetLastCursor(),
                                window_->GetLastCursor(), base::DoNothing());
}

void X11DesktopWindowMoveClient::EndMoveLoop() {
  move_loop_.EndMoveLoop();
}

}  // namespace ui
