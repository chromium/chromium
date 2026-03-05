// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/ui_controls.h"

#include <utility>

#include "base/check.h"
#include "base/functional/callback_helpers.h"
#include "ui/android/window_android.h"

namespace ui_controls {
namespace {

bool g_ui_controls_enabled = false;

void CheckUIControlsEnabled() {
  CHECK(g_ui_controls_enabled)
      << "In order to use ui_controls methods, you must be in a test "
         "executable that enables UI Controls. Currently, this is "
         "android_browsertests on Android platform.\n"
         "This limitation prevents attempting to send input that might require "
         "the test process to be active and focused in an environment where "
         "the process is not guaranteed to be running exclusively, which can "
         "lead to flaky tests.";
}

// Note: the order of the bool params are different from SendKeyPress family
// in order to make them aligned with other parts of UI flags, such as
// EventFlags and AcceleratorState.
bool SendKeyEventsInternal(gfx::NativeWindow window,
                           ui::KeyboardCode key,
                           int key_event_types,
                           bool shift,
                           bool control,
                           bool alt,
                           bool command,
                           base::OnceClosure task) {
  if (!window->SendKeyEventsForTesting(key, key_event_types, shift, control,
                                       alt, command)) {
    return false;
  }
  if (task) {
    std::move(task).Run();
  }
  return true;
}

}  // namespace

void EnableUIControls() {
  // This should be called at most once per process.
  CHECK(!g_ui_controls_enabled);
  g_ui_controls_enabled = true;
}

bool SendKeyPress(gfx::NativeWindow window,
                  ui::KeyboardCode key,
                  bool control,
                  bool shift,
                  bool alt,
                  bool command) {
  CheckUIControlsEnabled();

  return SendKeyEventsInternal(
      window, key, (KeyEventType::kKeyPress | KeyEventType::kKeyRelease), shift,
      control, alt, command, /*task=*/base::OnceClosure());
}

bool SendKeyPressNotifyWhenDone(gfx::NativeWindow window,
                                ui::KeyboardCode key,
                                bool control,
                                bool shift,
                                bool alt,
                                bool command,
                                base::OnceClosure task,
                                KeyEventType wait_for) {
  CheckUIControlsEnabled();

  // This doesn't time out if `window` is deleted before the key release events
  // are dispatched, so it's fine to ignore `wait_for` and always wait for key
  // release events. Just CHECK here.
  CHECK(wait_for == ui_controls::KeyEventType::kKeyPress ||
        wait_for == ui_controls::KeyEventType::kKeyRelease);
  return SendKeyEventsInternal(
      window, key, (KeyEventType::kKeyPress | KeyEventType::kKeyRelease), shift,
      control, alt, command, std::move(task));
}

bool SendKeyEvents(gfx::NativeWindow window,
                   ui::KeyboardCode key,
                   int key_event_types,
                   int accelerator_state) {
  CheckUIControlsEnabled();

  return SendKeyEventsInternal(
      window, key, key_event_types,
      /*shift=*/(accelerator_state & AcceleratorState::kShift),
      /*control=*/(accelerator_state & AcceleratorState::kControl),
      /*alt=*/(accelerator_state & AcceleratorState::kAlt),
      /*command=*/(accelerator_state & AcceleratorState::kCommand),
      /*task=*/base::NullCallback());
}

bool SendKeyEventsNotifyWhenDone(gfx::NativeWindow window,
                                 ui::KeyboardCode key,
                                 int key_event_types,
                                 base::OnceClosure task,
                                 int accelerator_state) {
  CheckUIControlsEnabled();

  return SendKeyEventsInternal(
      window, key, key_event_types,
      /*shift=*/(accelerator_state & AcceleratorState::kShift),
      /*control=*/(accelerator_state & AcceleratorState::kControl),
      /*alt=*/(accelerator_state & AcceleratorState::kAlt),
      /*command=*/(accelerator_state & AcceleratorState::kCommand),
      std::move(task));
}

}  // namespace ui_controls
