// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/events/keyboard_layout_util.h"

#include "ui/chromeos/events/event_rewriter_chromeos.h"
#include "ui/events/devices/device_data_manager.h"

namespace ui {

bool DeviceUsesKeyboardLayout2() {
  for (const InputDevice& keyboard :
       DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    if (EventRewriterChromeOS::GetKeyboardTopRowLayout(keyboard) ==
        EventRewriterChromeOS::kKbdTopRowLayout2) {
      return true;
    }
  }

  return false;
}

bool DeviceKeyboardHasAssistantKey() {
  for (const InputDevice& keyboard :
       DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    bool has_assistant_key = false;
    if (EventRewriterChromeOS::HasAssistantKeyOnKeyboard(keyboard,
                                                         &has_assistant_key) &&
        has_assistant_key) {
      return true;
    }
  }

  return false;
}

}  // namespace ui
