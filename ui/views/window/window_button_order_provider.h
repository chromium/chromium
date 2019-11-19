// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WINDOW_WINDOW_BUTTON_ORDER_PROVIDER_H_
#define UI_VIEWS_WINDOW_WINDOW_BUTTON_ORDER_PROVIDER_H_

#include <vector>

#include "base/macros.h"
#include "ui/views/views_export.h"
#include "ui/views/window/frame_buttons.h"

namespace views {

// Stores the ordering of window control buttons. Provides a default ordering
// of |kMinimize|, |FrameButton::kMaximize|, |FrameButton::kClose|, where all
// controls are on the trailing end of a window.
//
// On Linux users can provide configuration files to control the ordering. This
// configuration is checked and overrides the defaults.
class VIEWS_EXPORT WindowButtonOrderProvider {
 public:
  static WindowButtonOrderProvider* GetInstance();

  const std::vector<views::FrameButton>& leading_buttons() const {
    return leading_buttons_;
  }

  const std::vector<views::FrameButton>& trailing_buttons() const {
    return trailing_buttons_;
  }

  void SetWindowButtonOrder(
      const std::vector<views::FrameButton>& leading_buttons,
      const std::vector<views::FrameButton>& trailing_buttons);

 protected:
  WindowButtonOrderProvider();
  virtual ~WindowButtonOrderProvider();

 private:
  static WindowButtonOrderProvider* instance_;

  // Layout arrangement of the window caption buttons. On linux these will be
  // set via a WindowButtonOrderObserver. On other platforms a default
  // arrangement of a trailing minimize, maximize, close, will be set.
  std::vector<views::FrameButton> leading_buttons_;
  std::vector<views::FrameButton> trailing_buttons_;

  DISALLOW_COPY_AND_ASSIGN(WindowButtonOrderProvider);
};

}  // namespace views

#endif  // UI_VIEWS_WINDOW_WINDOW_BUTTON_ORDER_PROVIDER_H_
