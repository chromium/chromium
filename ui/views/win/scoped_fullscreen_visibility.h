// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIN_SCOPED_FULLSCREEN_VISIBILITY_H_
#define UI_VIEWS_WIN_SCOPED_FULLSCREEN_VISIBILITY_H_

#include <windows.h>

#include <map>

#include "ui/views/views_export.h"

namespace views {

// Scoping class that ensures a HWND remains hidden while it enters or leaves
// the fullscreen state. This reduces some flicker-jank that an application UI
// might suffer.
class VIEWS_EXPORT ScopedFullscreenVisibility {
 public:
  explicit ScopedFullscreenVisibility(HWND hwnd);

  ScopedFullscreenVisibility(const ScopedFullscreenVisibility&) = delete;
  ScopedFullscreenVisibility& operator=(const ScopedFullscreenVisibility&) =
      delete;

  ~ScopedFullscreenVisibility();

  // Returns true if |hwnd| is currently hidden due to instance(s) of this
  // class.
  static bool IsHiddenForFullscreen(HWND hwnd);

 private:
  using FullscreenHWNDs = std::map<HWND, int>;

  HWND hwnd_;

  static FullscreenHWNDs* full_screen_windows_;
};

}  // namespace views

#endif  // UI_VIEWS_WIN_SCOPED_FULLSCREEN_VISIBILITY_H_
