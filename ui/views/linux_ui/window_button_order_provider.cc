// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/window/window_button_order_provider.h"

#include "base/macros.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/linux_ui/window_button_order_observer.h"

namespace views {

namespace {

class WindowButtonOrderObserverDelegate : public WindowButtonOrderProvider,
                                          public WindowButtonOrderObserver {
 public:
  WindowButtonOrderObserverDelegate();
  ~WindowButtonOrderObserverDelegate() override;

  // WindowButtonOrderObserver:
  void OnWindowButtonOrderingChange(
      const std::vector<views::FrameButton>& leading_buttons,
      const std::vector<views::FrameButton>& trailing_buttons) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowButtonOrderObserverDelegate);
};

///////////////////////////////////////////////////////////////////////////////
// WindowButtonOrderObserverDelegate, public:

WindowButtonOrderObserverDelegate::WindowButtonOrderObserverDelegate() {
  views::LinuxUI* ui = views::LinuxUI::instance();
  if (ui)
    ui->AddWindowButtonOrderObserver(this);
}

WindowButtonOrderObserverDelegate::~WindowButtonOrderObserverDelegate() {
  views::LinuxUI* ui = views::LinuxUI::instance();
  if (ui)
    ui->RemoveWindowButtonOrderObserver(this);
}

void WindowButtonOrderObserverDelegate::OnWindowButtonOrderingChange(
    const std::vector<views::FrameButton>& leading_buttons,
    const std::vector<views::FrameButton>& trailing_buttons) {
  SetWindowButtonOrder(leading_buttons, trailing_buttons);
}

}  // namespace

// static
WindowButtonOrderProvider* WindowButtonOrderProvider::instance_ = nullptr;

///////////////////////////////////////////////////////////////////////////////
// WindowButtonOrderProvider, public:

// static
WindowButtonOrderProvider* WindowButtonOrderProvider::GetInstance() {
  if (!instance_)
    instance_ = new WindowButtonOrderObserverDelegate;
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
