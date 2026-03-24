// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/user_resize_move_detector.h"

#include <windows.h>

#include "ui/views/win/hwnd_message_handler_delegate.h"

namespace views {

UserResizeMoveDetector::UserResizeMoveDetector(
    HWNDMessageHandlerDelegate* hwnd_delegate)
    : hwnd_delegate_(hwnd_delegate) {}

void UserResizeMoveDetector::OnEnterSizeMove() {
  if (state_ == State::kNotResizing) {
    state_ = State::kInSizeMove;
  }
}

void UserResizeMoveDetector::OnSizing() {
  if (state_ == State::kInSizeMove) {
    state_ = State::kInSizing;
    hwnd_delegate_->HandleBeginUserResize();
  }
}

void UserResizeMoveDetector::OnMoving() {
  if (state_ == State::kInSizeMove) {
    state_ = State::kInMoving;
    hwnd_delegate_->HandleBeginUserDrag();
  }
}

void UserResizeMoveDetector::OnExitSizeMove() {
  if (state_ == State::kInSizing) {
    hwnd_delegate_->HandleEndUserResize();
  } else if (state_ == State::kInMoving) {
    hwnd_delegate_->HandleEndUserDrag();
  }
  state_ = State::kNotResizing;
}

}  // namespace views
