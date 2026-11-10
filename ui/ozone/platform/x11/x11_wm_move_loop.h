// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_WM_MOVE_LOOP_H_
#define UI_OZONE_PLATFORM_X11_X11_WM_MOVE_LOOP_H_

#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/x/event.h"
#include "ui/gfx/x/event_observer.h"
#include "ui/gfx/x/xproto.h"

namespace x11 {
class Connection;
}

namespace ui {

// Kill switch for using window-manager-driven moves for tab dragging.
BASE_DECLARE_FEATURE(kX11WmMoveLoop);

// Runs a nested run loop while the window manager interactively moves a window
// in response to a _NET_WM_MOVERESIZE request. Window managers like Mutter
// constrain windows moved by the client (via ConfigureRequest) to the work
// area, but moves performed by the window manager itself count as user actions
// and may place the window partially offscreen.
//
// The loop ends when the left mouse button is released or when End() is
// called. Button releases are detected with XInput2 raw events on the root
// window since those are delivered even while the window manager holds the
// pointer grab.
class X11WmMoveLoop : public x11::EventObserver {
 public:
  X11WmMoveLoop(x11::Connection& connection,
                x11::Window root_window,
                x11::Window window);
  X11WmMoveLoop(const X11WmMoveLoop&) = delete;
  X11WmMoveLoop& operator=(const X11WmMoveLoop&) = delete;
  ~X11WmMoveLoop() override;

  // Returns true if the running window manager is known to handle
  // window-manager-driven moves well enough to use them for tab dragging.
  static bool IsSupported(x11::Connection& connection);

  // Asks the window manager to move the window, keeping the point at
  // `drag_offset` (in pixels, relative to the window's origin) under the
  // pointer, and runs until the move ends. If `window_mapped` is false, the
  // move starts once the window is mapped. Returns false if `this` was
  // destroyed during the loop.
  bool Run(const gfx::Vector2d& drag_offset, bool window_mapped);

  // Ends the move loop, cancelling the window manager's move if it is still
  // in progress.
  void End();

 private:
  // x11::EventObserver:
  void OnEvent(const x11::Event& event) override;

  void StartWmMove();
  void SelectRawButtonReleaseEvents(bool select);
  bool IsLeftButtonDown();

  const raw_ref<x11::Connection> connection_;
  const x11::Window root_window_;
  const x11::Window window_;

  gfx::Vector2d drag_offset_;
  bool in_move_loop_ = false;
  bool wm_move_started_ = false;
  base::OnceClosure quit_closure_;

  base::WeakPtrFactory<X11WmMoveLoop> weak_factory_{this};
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_WM_MOVE_LOOP_H_
