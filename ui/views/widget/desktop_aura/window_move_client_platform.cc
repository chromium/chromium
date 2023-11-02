// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/window_move_client_platform.h"

#include "ui/views/widget/desktop_aura/desktop_window_tree_host_platform.h"

namespace views {

WindowMoveClientPlatform::WindowMoveClientPlatform(
    DesktopWindowTreeHostPlatform* host)
    : host_(host) {}

WindowMoveClientPlatform::~WindowMoveClientPlatform() = default;

wm::WindowMoveResult WindowMoveClientPlatform::RunMoveLoop(
    aura::Window* source,
    const gfx::Vector2d& drag_offset,
    wm::WindowMoveSource move_source) {
  DCHECK(host_->GetContentWindow()->Contains(source));
  auto move_loop_result = host_->RunMoveLoop(
      drag_offset,
      move_source == wm::WindowMoveSource::WINDOW_MOVE_SOURCE_MOUSE
          ? Widget::MoveLoopSource::kMouse
          : Widget::MoveLoopSource::kTouch,
      Widget::MoveLoopEscapeBehavior::kHide);

  return move_loop_result == Widget::MoveLoopResult::kSuccessful
             ? wm::MOVE_SUCCESSFUL
             : wm::MOVE_CANCELED;
}

void WindowMoveClientPlatform::EndMoveLoop() {
  host_->EndMoveLoop();
}

}  // namespace views
