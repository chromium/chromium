// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/scoped_fullscreen_visibility.h"

#include "base/logging.h"

namespace views {

// static
std::map<HWND, int>* ScopedFullscreenVisibility::full_screen_windows_ = nullptr;

ScopedFullscreenVisibility::ScopedFullscreenVisibility(HWND hwnd)
    : hwnd_(hwnd) {
  if (!full_screen_windows_)
    full_screen_windows_ = new FullscreenHWNDs;
  FullscreenHWNDs::iterator it = full_screen_windows_->find(hwnd_);
  if (it != full_screen_windows_->end()) {
    it->second++;
  } else {
    full_screen_windows_->insert(std::make_pair(hwnd_, 1));
    // NOTE: Be careful not to activate any windows here (for example, calling
    // ShowWindow(SW_HIDE) will automatically activate another window).  This
    // code can be called while a window is being deactivated, and activating
    // another window will screw up the activation that is already in progress.
    SetWindowPos(hwnd_, nullptr, 0, 0, 0, 0,
                 SWP_HIDEWINDOW | SWP_NOACTIVATE | SWP_NOMOVE |
                     SWP_NOREPOSITION | SWP_NOSIZE | SWP_NOZORDER);
  }
}

ScopedFullscreenVisibility::~ScopedFullscreenVisibility() {
  FullscreenHWNDs::iterator it = full_screen_windows_->find(hwnd_);
  DCHECK(it != full_screen_windows_->end());
  if (--it->second == 0) {
    full_screen_windows_->erase(it);
    ShowWindow(hwnd_, SW_SHOW);
  }
  if (full_screen_windows_->empty()) {
    delete full_screen_windows_;
    full_screen_windows_ = nullptr;
  }
}

// static
bool ScopedFullscreenVisibility::IsHiddenForFullscreen(HWND hwnd) {
  if (!full_screen_windows_)
    return false;
  return full_screen_windows_->find(hwnd) != full_screen_windows_->end();
}

}  // namespace views
