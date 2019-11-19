// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/window_button_order_provider.h"

namespace views {

// static
WindowButtonOrderProvider* WindowButtonOrderProvider::instance_ = nullptr;

///////////////////////////////////////////////////////////////////////////////
// WindowButtonOrderProvider, public:

// static
WindowButtonOrderProvider* WindowButtonOrderProvider::GetInstance() {
  if (!instance_)
    instance_ = new WindowButtonOrderProvider;
  return instance_;
}

///////////////////////////////////////////////////////////////////////////////
// WindowButtonOrderProvider, protected:

WindowButtonOrderProvider::WindowButtonOrderProvider() {
  trailing_buttons_.push_back(views::FrameButton::kMinimize);
  trailing_buttons_.push_back(views::FrameButton::kMaximize);
  trailing_buttons_.push_back(views::FrameButton::kClose);
}

WindowButtonOrderProvider::~WindowButtonOrderProvider() = default;

void WindowButtonOrderProvider::SetWindowButtonOrder(
    const std::vector<views::FrameButton>& leading_buttons,
    const std::vector<views::FrameButton>& trailing_buttons) {
  leading_buttons_ = leading_buttons;
  trailing_buttons_ = trailing_buttons;
}

}  // namespace views
