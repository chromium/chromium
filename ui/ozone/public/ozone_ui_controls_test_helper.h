// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_OZONE_UI_CONTROLS_TEST_HELPER_H_
#define UI_OZONE_PUBLIC_OZONE_UI_CONTROLS_TEST_HELPER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/component_export.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Point;
}

namespace ui {

class OzoneUIControlsTestHelper {
 public:
  virtual ~OzoneUIControlsTestHelper() = default;

  // Returns current button down mask.
  virtual unsigned ButtonDownMask() const = 0;

  // Sends key press event and executes |closure| when done.
  virtual void SendKeyPressEvent(gfx::AcceleratedWidget widget,
                                 ui::KeyboardCode key,
                                 bool control,
                                 bool shift,
                                 bool alt,
                                 bool command,
                                 base::OnceClosure closure) = 0;

  // Sends mouse motion notify event and executes |closure| when done.
  virtual void SendMouseMotionNotifyEvent(gfx::AcceleratedWidget widget,
                                          const gfx::Point& mouse_loc,
                                          const gfx::Point& mouse_root_loc,
                                          base::OnceClosure closure) = 0;

  // Sends mouse event and executes |closure| when done.
  virtual void SendMouseEvent(gfx::AcceleratedWidget widget,
                              ui_controls::MouseButton type,
                              int button_state,
                              int accelerator_state,
                              const gfx::Point& mouse_loc,
                              const gfx::Point& mouse_root_loc,
                              base::OnceClosure closure) = 0;

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Sends touch event and executes |closure| when done.
  virtual void SendTouchEvent(gfx::AcceleratedWidget widget,
                              int action,
                              int id,
                              const gfx::Point& touch_loc,
                              base::OnceClosure closure) = 0;
#endif

  // Executes closure after all pending ui events are sent.
  virtual void RunClosureAfterAllPendingUIEvents(base::OnceClosure closure) = 0;

  // Tells the client of OzoneUIControlsTestHelper that it must use
  // SendMouseMotionNotifyEvent instead of calling MoveCursorTo via
  // aura::Window.
  virtual bool MustUseUiControlsForMoveCursorTo() = 0;
};

COMPONENT_EXPORT(OZONE)
std::unique_ptr<OzoneUIControlsTestHelper> CreateOzoneUIControlsTestHelper();

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_OZONE_UI_CONTROLS_TEST_HELPER_H_
