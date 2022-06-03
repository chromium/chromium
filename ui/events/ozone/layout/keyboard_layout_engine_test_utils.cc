// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/layout/keyboard_layout_engine_test_utils.h"

#include "base/callback.h"
#include "base/run_loop.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/keyboard_layout_engine_manager.h"

namespace ui {

// TODO(crbug.com/1209477): Wayland bots use Weston with Headless backend that
// sets up XkbKeyboardLayoutEngine differently. When that is fixed, remove the
// workaround function below.
void WaitUntilLayoutEngineIsReadyForTest() {
  // The platform may set the keyboard layout asynchronously.  We need the
  // layout when handling key events, so wait until it is ready.
  //
  // See crbug.com/1186996
  base::RunLoop loop;
  ui::KeyboardLayoutEngineManager::GetKeyboardLayoutEngine()
      ->SetInitCallbackForTest(loop.QuitClosure());
  loop.Run();
}

}  // namespace ui
