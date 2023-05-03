// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ash/keyboard_layout_util.h"

#include "ui/events/ash/event_rewriter_ash.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/keyboard_device.h"

namespace ui {

bool DeviceKeyboardHasAssistantKey() {
  for (const KeyboardDevice& keyboard :
       DeviceDataManager::GetInstance()->GetKeyboardDevices()) {
    bool has_assistant_key = false;
    if (EventRewriterAsh::HasAssistantKeyOnKeyboard(keyboard,
                                                    &has_assistant_key) &&
        has_assistant_key) {
      return true;
    }
  }

  return false;
}

}  // namespace ui
