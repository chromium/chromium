// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/ui_controls_aura.h"

#include "base/check.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "ui/gfx/native_ui_types.h"

namespace ui_controls {
namespace {
UIControlsAura* g_instance = nullptr;
bool g_ui_controls_enabled = false;

void CheckUIControlsEnabled() {
  CHECK(g_ui_controls_enabled)
      << "In order to use ui_controls methods, you must be in a test "
         "executable that enables UI Controls. Currently, this is "
         "interactive_ui_tests and some fuzzing tests.\n"
         "This limitation prevents attempting to send input that might require "
         "the test process to be active and focused in an environment where "
         "the process is not guaranteed to be running exclusively, which can "
         "lead to flaky tests.";
}

}  // namespace

#if !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_WIN)
void EnableUIControls() {
  g_ui_controls_enabled = true;
}
#endif  // !BUILDFLAG(IS_CHROMEOS) && !BUILDFLAG(IS_WIN)

void ResetUIControlsIfEnabled() {}

// An interface to provide Aura implementation of UI control.

// static
bool SendKeyPress(gfx::NativeWindow window,
                  ui::KeyboardCode key,
                  bool control,
                  bool shift,
                  bool alt,
                  bool command) {
  CheckUIControlsEnabled();
  return g_instance->SendKeyEvents(
      window, key, kKeyPress | kKeyRelease,
      GenerateAcceleratorState(control, shift, alt, command));
}

// static
bool SendKeyPressNotifyWhenDone(gfx::NativeWindow window,
                                ui::KeyboardCode key,
                                bool control,
                                bool shift,
                                bool alt,
                                bool command,
                                base::OnceClosure task,
                                KeyEventType wait_for) {
  CheckUIControlsEnabled();
  CHECK(wait_for == ui_controls::KeyEventType::kKeyPress ||
        wait_for == ui_controls::KeyEventType::kKeyRelease);
  return g_instance->SendKeyEventsNotifyWhenDone(
      window, key, kKeyPress | kKeyRelease, std::move(task),
      GenerateAcceleratorState(control, shift, alt, command), wait_for);
}

// static
bool SendKeyEvents(gfx::NativeWindow window,
                   ui::KeyboardCode key,
                   int key_event_types,
                   int accelerator_state) {
  CheckUIControlsEnabled();

  // Make sure `key_event_types` abd `accelerator_state` is valid.
  // `key_event_types` must include at least one key event type.
  DCHECK(key_event_types > 0 && key_event_types <= (kKeyPress | kKeyRelease));
  DCHECK(accelerator_state >= 0 &&
         accelerator_state <= (kShift | kControl | kAlt | kCommand));

  return g_instance->SendKeyEvents(window, key, key_event_types,
                                   accelerator_state);
}

// static
bool SendKeyEventsNotifyWhenDone(gfx::NativeWindow window,
                                 ui::KeyboardCode key,
                                 int key_event_types,
                                 base::OnceClosure task,
                                 int accelerator_state) {
  CheckUIControlsEnabled();

  // Make sure `key_event_types` abd `accelerator_state` is valid.
  // `key_event_types` must include at least one key event type.
  DCHECK(key_event_types > 0 && key_event_types <= (kKeyPress | kKeyRelease));
  DCHECK(accelerator_state >= 0 &&
         accelerator_state <= (kShift | kControl | kAlt | kCommand));

  return g_instance->SendKeyEventsNotifyWhenDone(
      window, key, key_event_types, std::move(task), accelerator_state,
      KeyEventType::kKeyPress);
}

// static
bool SendMouseMove(int x, int y, gfx::NativeWindow) {
  // TODO(crbug.com/40249511): Maybe use the window hint on other platforms.
  CheckUIControlsEnabled();
  return g_instance->SendMouseMove(x, y);
}

// static
bool SendMouseMoveNotifyWhenDone(int x,
                                 int y,
                                 base::OnceClosure task,
                                 gfx::NativeWindow) {
  // TODO(crbug.com/40249511): Maybe use the window hint on other platforms.
  CheckUIControlsEnabled();
  return g_instance->SendMouseMoveNotifyWhenDone(x, y, std::move(task));
}

// static
bool SendMouseEvents(MouseButton type,
                     int button_state,
                     int accelerator_state,
                     gfx::NativeWindow) {
  // TODO(crbug.com/40249511): Maybe use the window hint on other platforms.
  CheckUIControlsEnabled();
  return g_instance->SendMouseEvents(type, button_state, accelerator_state);
}

// static
bool SendMouseEventsNotifyWhenDone(MouseButton type,
                                   int button_state,
                                   base::OnceClosure task,
                                   int accelerator_state,
                                   gfx::NativeWindow) {
  // TODO(crbug.com/40249511): Maybe use the window hint on other platforms.
  CheckUIControlsEnabled();
  return g_instance->SendMouseEventsNotifyWhenDone(
      type, button_state, std::move(task), accelerator_state);
}

// static
bool SendMouseClick(MouseButton type, gfx::NativeWindow) {
  // TODO(crbug.com/40249511): Do any Aura platforms need to use the hint?
  CheckUIControlsEnabled();
  return g_instance->SendMouseClick(type);
}

#if BUILDFLAG(IS_WIN)
// static
bool SendTouchEvents(int action, int num, int x, int y) {
  CheckUIControlsEnabled();
  return g_instance->SendTouchEvents(action, num, x, y);
}
#elif BUILDFLAG(IS_CHROMEOS)
// static
bool SendTouchEvents(int action, int id, int x, int y) {
  return SendTouchEventsNotifyWhenDone(action, id, x, y, base::OnceClosure());
}

// static
bool SendTouchEventsNotifyWhenDone(int action,
                                   int id,
                                   int x,
                                   int y,
                                   base::OnceClosure task) {
  CheckUIControlsEnabled();
  return g_instance->SendTouchEventsNotifyWhenDone(action, id, x, y,
                                                   std::move(task));
}
#endif

UIControlsAura::UIControlsAura() {
}

UIControlsAura::~UIControlsAura() {
}

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
// static. declared in ui_controls.h
void InstallUIControlsAura(UIControlsAura* instance) {
  g_ui_controls_enabled = true;
  delete g_instance;
  g_instance = instance;
}
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)

}  // namespace ui_controls
