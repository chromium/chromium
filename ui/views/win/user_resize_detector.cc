// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/user_resize_detector.h"

#include <windows.h>

#include "ui/views/win/hwnd_message_handler_delegate.h"

namespace views {

UserResizeDetector::UserResizeDetector(
    HWNDMessageHandlerDelegate* hwnd_delegate)
    : hwnd_delegate_(hwnd_delegate) {}

void UserResizeDetector::OnEnterSizeMove() {
  if (state_ == State::kNotResizing) {
    state_ = State::kInSizeMove;
  }
}

void UserResizeDetector::OnSizing() {
  if (state_ == State::kInSizeMove) {
    state_ = State::kInSizing;
    hwnd_delegate_->HandleBeginUserResize();
  }
}

void UserResizeDetector::OnExitSizeMove() {
  if (state_ == State::kInSizing) {
    hwnd_delegate_->HandleEndUserResize();
  }
  state_ = State::kNotResizing;
}

}  // namespace views
