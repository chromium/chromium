// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_X_TEST_X11_UI_CONTROLS_TEST_HELPER_H_
#define UI_BASE_X_TEST_X11_UI_CONTROLS_TEST_HELPER_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/test/ui_controls_aura.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/connection.h"

namespace gfx {
class Point;
}

namespace ui {

class COMPONENT_EXPORT(UI_BASE_X) X11UIControlsTestHelper {
 public:
  X11UIControlsTestHelper();
  X11UIControlsTestHelper(const X11UIControlsTestHelper&) = delete;
  X11UIControlsTestHelper& operator=(const X11UIControlsTestHelper&) = delete;
  ~X11UIControlsTestHelper();

  unsigned ButtonDownMask() const;

  // Sends key events and executes |closure| when done.
  void SendKeyEvents(gfx::AcceleratedWidget widget,
                     ui::KeyboardCode key,
                     int key_event_types,
                     int accelerator_state,
                     base::OnceClosure closure);

  // Sends mouse motion notify event and executes |closure| when done.
  void SendMouseMotionNotifyEvent(gfx::AcceleratedWidget widget,
                                  const gfx::Point& mouse_loc,
                                  const gfx::Point& mouse_loc_in_screen_px,
                                  base::OnceClosure closure);

  // Sends mouse event and executes |closure| when done.
  void SendMouseEvent(gfx::AcceleratedWidget widget,
                      ui_controls::MouseButton type,
                      int button_state,
                      int accelerator_state,
                      const gfx::Point& mouse_loc,
                      const gfx::Point& mouse_loc_in_screen_px,
                      base::OnceClosure closure);

  void RunClosureAfterAllPendingUIEvents(base::OnceClosure closure);

 private:
  void SetKeycodeAndSendThenMask(gfx::AcceleratedWidget widget,
                                 x11::KeyEvent* xevent,
                                 uint32_t keysym,
                                 x11::KeyButMask mask);

  void UnmaskAndSetKeycodeThenSend(gfx::AcceleratedWidget widget,
                                   x11::KeyEvent* xevent,
                                   x11::KeyButMask mask,
                                   uint32_t keysym);

  // Our X11 state.
  raw_ref<x11::Connection> connection_;
  x11::Window x_root_window_;

  // Input-only window used for events.
  x11::Window x_window_;
};

}  // namespace ui

#endif  // UI_BASE_X_TEST_X11_UI_CONTROLS_TEST_HELPER_H_
