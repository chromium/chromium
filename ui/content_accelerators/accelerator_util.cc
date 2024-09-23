// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/content_accelerators/accelerator_util.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace ui {

ui::Accelerator GetAcceleratorFromNativeWebKeyboardEvent(
    const input::NativeWebKeyboardEvent& event) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (::features::IsImprovedKeyboardShortcutsEnabled()) {
    // TODO: This must be the same as below and it's simpler.
    // Cleanup if this change sticks.
    auto* os_event = static_cast<ui::KeyEvent*>(event.os_event);

    // If there is no |os_event| fall through to the default code path.
    // This can occur when keys are injected from dev tools.
    if (os_event)
      return ui::Accelerator(*os_event);
  }
#endif
  Accelerator::KeyState key_state =
      event.GetType() == blink::WebInputEvent::Type::kKeyUp
          ? Accelerator::KeyState::RELEASED
          : Accelerator::KeyState::PRESSED;
  ui::KeyboardCode keyboard_code =
      static_cast<ui::KeyboardCode>(event.windows_key_code);
  int modifiers = WebEventModifiersToEventFlags(event.GetModifiers());
  return ui::Accelerator(keyboard_code, modifiers, key_state,
                         event.TimeStamp());
}

}  // namespace ui
