// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/ui_controls.h"

namespace ui_controls {

int GenerateAcceleratorState(bool control, bool shift, bool alt, bool command) {
  int accelerator_state = ui_controls::kNoAccelerator;
  if (control) {
    accelerator_state |= ui_controls::kControl;
  }
  if (shift) {
    accelerator_state |= ui_controls::kShift;
  }
  if (alt) {
    accelerator_state |= ui_controls::kAlt;
  }
  if (command) {
    accelerator_state |= ui_controls::kCommand;
  }

  return accelerator_state;
}

}  // namespace ui_controls
