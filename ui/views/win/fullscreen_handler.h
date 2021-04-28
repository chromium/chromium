// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIN_FULLSCREEN_HANDLER_H_
#define UI_VIEWS_WIN_FULLSCREEN_HANDLER_H_

#include <shobjidl.h>
#include <wrl/client.h>

#include <map>

#include "base/macros.h"

namespace gfx {
class Rect;
}

namespace views {

class FullscreenHandler {
 public:
  FullscreenHandler();
  ~FullscreenHandler();

  void set_hwnd(HWND hwnd) { hwnd_ = hwnd; }

  void SetFullscreen(bool fullscreen);

  // Informs the taskbar whether the window is a fullscreen window.
  void MarkFullscreen(bool fullscreen);

  gfx::Rect GetRestoreBounds() const;

  bool fullscreen() const { return fullscreen_; }

 private:
  // Information saved before going into fullscreen mode, used to restore the
  // window afterwards.
  struct SavedWindowInfo {
    LONG style;
    LONG ex_style;
    RECT window_rect;
  };

  void SetFullscreenImpl(bool fullscreen);

  HWND hwnd_ = nullptr;
  bool fullscreen_ = false;

  // Saved window information from before entering fullscreen mode.
  // TODO(beng): move to private once GetRestoredBounds() moves onto Widget.
  SavedWindowInfo saved_window_info_;
  // Used to mark a window as fullscreen.
  Microsoft::WRL::ComPtr<ITaskbarList2> task_bar_list_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenHandler);
};

}  // namespace views

#endif  // UI_VIEWS_WIN_FULLSCREEN_HANDLER_H_
