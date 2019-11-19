// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_SYSTEM_INPUT_INJECTOR_H_
#define UI_OZONE_PUBLIC_SYSTEM_INPUT_INJECTOR_H_

#include "base/component_export.h"
#include "base/macros.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace gfx {
class PointF;
}  // namespace gfx

namespace ui {

// Interface exposed for remoting to convert its events from the network to
// native events.
class COMPONENT_EXPORT(OZONE) SystemInputInjector {
 public:
  SystemInputInjector() {}
  virtual ~SystemInputInjector() {}

  // Moves the cursor on the screen and generates the corresponding MouseMove or
  // MouseDragged event.  |location| is in physical screen coordinates,
  // independent of the scale factor and the display rotation settings.
  virtual void MoveCursorTo(const gfx::PointF& location) = 0;

  // Simulates a mouse button click.  |button| must be one of
  // EF_LEFT_MOUSE_BUTTON, EF_RIGHT_MOUSE_BUTTON or EF_MIDDLE_MOUSE_BUTTON.
  // SystemInputInjector will apply the correct modifiers (shift, ctrl, etc).
  virtual void InjectMouseButton(EventFlags button, bool down) = 0;

  // |delta_x| and |delta_y| are in physical pixels independent of the scale
  // factor.
  virtual void InjectMouseWheel(int delta_x, int delta_y) = 0;

  // Simulates a key event.  SystemInputInjector maps |physical_key| to the
  // correct logical key based on the current keyboard layout. |down| is true
  // for presses. If |suppress_auto_repeat| is set, the platform must not
  // auto-repeat the event.
  virtual void InjectKeyEvent(DomCode physical_key,
                              bool down,
                              bool suppress_auto_repeat) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemInputInjector);
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_SYSTEM_INPUT_INJECTOR_H_
