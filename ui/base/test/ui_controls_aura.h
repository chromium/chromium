// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_UI_CONTROLS_AURA_H_
#define UI_BASE_TEST_UI_CONTROLS_AURA_H_

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/base/test/ui_controls.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/native_widget_types.h"

namespace ui_controls {

// An interface to provide Aura implementation of UI control.
class UIControlsAura {
 public:
  UIControlsAura();
  virtual ~UIControlsAura();

  // Sends a key press and/or release message.
  virtual bool SendKeyEvents(gfx::NativeWindow window,
                             ui::KeyboardCode key,
                             int key_event_types,
                             int accerelator_state) = 0;
  virtual bool SendKeyEventsNotifyWhenDone(gfx::NativeWindow window,
                                           ui::KeyboardCode key,
                                           int key_event_types,
                                           base::OnceClosure task,
                                           int accelerator_state,
                                           KeyEventType wait_for) = 0;

  // Simulate a mouse move. (x,y) are absolute screen coordinates.
  virtual bool SendMouseMove(int x, int y) = 0;
  virtual bool SendMouseMoveNotifyWhenDone(int x,
                                           int y,
                                           base::OnceClosure task) = 0;

  // Sends a mouse down and/or up message. The click will be sent to wherever
  // the cursor currently is, so be sure to move the cursor before calling this
  // (and be sure the cursor has arrived!).
  virtual bool SendMouseEvents(MouseButton type,
                               int button_state,
                               int accelerator_state) = 0;
  virtual bool SendMouseEventsNotifyWhenDone(MouseButton type,
                                             int button_state,
                                             base::OnceClosure task,
                                             int accelerator_state) = 0;
  // Same as SendMouseEvents with BUTTON_UP | BUTTON_DOWN.
  virtual bool SendMouseClick(MouseButton type) = 0;

#if BUILDFLAG(IS_WIN)
  virtual bool SendTouchEvents(int action, int num, int x, int y) = 0;
#elif BUILDFLAG(IS_CHROMEOS)
  virtual bool SendTouchEvents(int action, int id, int x, int y) = 0;
  virtual bool SendTouchEventsNotifyWhenDone(int action,
                                             int id,
                                             int x,
                                             int y,
                                             base::OnceClosure task) = 0;
#endif
};

}  // namespace ui_controls

#endif  // UI_BASE_TEST_UI_CONTROLS_AURA_H_
