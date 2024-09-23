// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIN_FULLSCREEN_HANDLER_H_
#define UI_VIEWS_WIN_FULLSCREEN_HANDLER_H_

#include <shobjidl.h>

#include <wrl/client.h>

#include "base/memory/weak_ptr.h"

namespace gfx {
class Rect;
}

namespace views {

class FullscreenHandler {
 public:
  FullscreenHandler();

  FullscreenHandler(const FullscreenHandler&) = delete;
  FullscreenHandler& operator=(const FullscreenHandler&) = delete;

  ~FullscreenHandler();

  void set_hwnd(HWND hwnd) { hwnd_ = hwnd; }

  // Set the fullscreen state. `target_display_id` indicates the display where
  // the window should be shown fullscreen; display::kInvalidDisplayId indicates
  // that no display was specified, so the current display may be used.
  void SetFullscreen(bool fullscreen, int64_t target_display_id);

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
    RECT rect;
    int dpi;
    HMONITOR monitor;
    MONITORINFO monitor_info;
  };

  void ProcessFullscreen(bool fullscreen, int64_t target_display_id);

  HWND hwnd_ = nullptr;
  bool fullscreen_ = false;

  // Saved window information from before entering fullscreen mode.
  // TODO(beng): move to private once GetRestoredBounds() moves onto Widget.
  SavedWindowInfo saved_window_info_;
  // Used to mark a window as fullscreen.
  Microsoft::WRL::ComPtr<ITaskbarList2> task_bar_list_;

  base::WeakPtrFactory<FullscreenHandler> weak_ptr_factory_{this};
};

}  // namespace views

#endif  // UI_VIEWS_WIN_FULLSCREEN_HANDLER_H_
