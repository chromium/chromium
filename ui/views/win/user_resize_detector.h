// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIN_USER_RESIZE_DETECTOR_H_
#define UI_VIEWS_WIN_USER_RESIZE_DETECTOR_H_

#include "base/memory/raw_ptr.h"

namespace views {

class HWNDMessageHandlerDelegate;

// This class detects the start and end of user resizing and notifies the HWND
// message handle delegate.
class UserResizeDetector {
 public:
  explicit UserResizeDetector(HWNDMessageHandlerDelegate* hwnd_delegate);

  // Called on WM_ENTERSIZEMOVE.
  void OnEnterSizeMove();

  // Called on WM_SIZING.
  void OnSizing();

  // Called on WM_EXITSIZEMOVE.
  void OnExitSizeMove();

 private:
  enum class State {
    // Start with not resizing.
    kNotResizing = 0,
    // When seeing WM_ENTERSIZEMOVE followed by WM_SIZING, the user has started
    // dragging the resize handle.
    kInSizeMove = 1,
    kInSizing = 2,
    // When seeing WM_EXITSIZEMOVE, the user has ended resizing the window.
    kExitSizeMove = kNotResizing
  } state_ = State::kNotResizing;

  raw_ptr<HWNDMessageHandlerDelegate> hwnd_delegate_;
};

}  // namespace views

#endif  // UI_VIEWS_WIN_USER_RESIZE_DETECTOR_H_
