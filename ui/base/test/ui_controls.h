// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_UI_CONTROLS_H_
#define UI_BASE_TEST_UI_CONTROLS_H_

#include <cstdint>
#include <string>

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/native_widget_types.h"

namespace ui_controls {

enum KeyEventType { kKeyPress = 1 << 0, kKeyRelease = 1 << 1 };

// A set of utility functions to generate native events in platform
// independent way. Note that since the implementations depend on a window being
// top level, these can only be called from test suites that are not sharded.
// For aura tests, please look into |aura::test:EventGenerator| first. This
// class provides a way to emulate events in synchronous way and it is often
// easier to write tests with this class than using |ui_controls|.
//
// Many of the functions in this class include a variant that takes a Closure.
// The version that takes a Closure waits until the generated event is
// processed. Once the generated event is processed the Closure is Run (and
// deleted). Note that this is a somewhat fragile process in that any event of
// the correct type (key down, mouse click, etc.) will trigger the Closure to be
// run. Hence a usage such as
//
//   SendKeyPress(...);
//   SendKeyPressNotifyWhenDone(..., task);
//
// might trigger |task| early.
//
// Note: Windows does not currently do anything with the |window| argument for
// these functions, so passing NULL is ok.

// Per the above comment, these methods can only be called from non-sharded test
// suites. This method ensures that they're not accidently called by sharded
// tests.
void EnableUIControls();

// Reset the state in ui controls logic that are updated by the test to the
// initial state.
void ResetUIControlsIfEnabled();

#if BUILDFLAG(IS_APPLE)
bool IsUIControlsEnabled();
#endif

// Generates keyboard accelerator state in bitmap from each key boolean.
int GenerateAcceleratorState(bool control, bool shift, bool alt, bool command);

// Send a key press with/without modifier keys. This will trigger a key release
// event after the key press.
//
// If you're writing a test chances are you want the variant in ui_test_utils.
// See it for details.
bool SendKeyPress(gfx::NativeWindow window,
                  ui::KeyboardCode key,
                  bool control,
                  bool shift,
                  bool alt,
                  bool command);
bool SendKeyPressNotifyWhenDone(
    gfx::NativeWindow window,
    ui::KeyboardCode key,
    bool control,
    bool shift,
    bool alt,
    bool command,
    base::OnceClosure task,
    KeyEventType wait_for = KeyEventType::kKeyRelease);

// The keys that may be held down while generating a keyboard/mouse event.
enum AcceleratorState {
  kNoAccelerator = 0,
  kShift = 1 << 0,
  kControl = 1 << 1,
  kAlt = 1 << 2,
  kCommand = 1 << 3,
};

// Not supported on win.
// TODO(crbug.com/40256300): Support this on win.
#if !BUILDFLAG(IS_WIN)
// Sends a key press and/or release message with/without modifier keys.
// `key_event_types` is a bitmask of KeyEventType constants that indicates what
// events are generated.
bool SendKeyEvents(gfx::NativeWindow window,
                   ui::KeyboardCode key,
                   int key_event_types,
                   int accelerator_state = kNoAccelerator);
bool SendKeyEventsNotifyWhenDone(gfx::NativeWindow window,
                                 ui::KeyboardCode key,
                                 int key_event_types,
                                 base::OnceClosure task,
                                 int accelerator_state = kNoAccelerator);
#endif  // !BUILDFLAG(IS_WIN)

// This value specifies that no window hint is given and an appropriate target
// window should be deduced from the target or current mouse position.
constexpr gfx::NativeWindow kNoWindowHint = gfx::NativeWindow();

// Simulate a mouse move.
//
// The `window_hint` - if specified - helps the method correctly target the
// appropriate window on platforms where mouse events must be explicitly
// targeted.
//
// Returns false on Windows if the desired position is not over a window
// belonging to the current process.
bool SendMouseMove(int screen_x,
                   int screen_y,
                   gfx::NativeWindow window_hint = kNoWindowHint);
bool SendMouseMoveNotifyWhenDone(int screen_x,
                                 int screen_y,
                                 base::OnceClosure task,
                                 gfx::NativeWindow window_hint = kNoWindowHint);

enum MouseButton {
  LEFT = 0,
  MIDDLE,
  RIGHT,
};

// Used to indicate the state of the button when generating events.
enum MouseButtonState {
  UP = 1,
  DOWN = 2
};

enum TouchType {
  kTouchPress = 1 << 0,
  kTouchRelease = 1 << 1,
  kTouchMove = 1 << 2,
};

// Sends a mouse down and/or up message with optional one or multiple
// accelerator keys. The click will be sent to wherever the cursor
// currently is, so be sure to move the cursor before calling this
// (and be sure the cursor has arrived!).
// |accelerator_state| is a bitmask of AcceleratorState.
//
// The `window_hint` - if specified - helps the method correctly target the
// appropriate window on platforms where mouse events must be explicitly
// targeted.
bool SendMouseEvents(MouseButton type,
                     int button_state,
                     int accelerator_state = kNoAccelerator,
                     gfx::NativeWindow window_hint = kNoWindowHint);
bool SendMouseEventsNotifyWhenDone(
    MouseButton type,
    int button_state,
    base::OnceClosure task,
    int accelerator_state = kNoAccelerator,
    gfx::NativeWindow window_hint = kNoWindowHint);

// Same as SendMouseEvents with UP | DOWN.
bool SendMouseClick(MouseButton type,
                    gfx::NativeWindow window_hint = kNoWindowHint);

#if BUILDFLAG(IS_WIN)
// Send WM_POINTER messages to generate touch events. There is no way to detect
// when events are received by chrome, it's up to users of this API to detect
// when events arrive. |action| is a bitmask of the TouchType constants that
// indicate what events are generated, |num| is the number of the touch
// pointers, |screen_x| and |screen_y| are the screen coordinates of a touch
// pointer.
bool SendTouchEvents(int action, int num, int screen_x, int screen_y);
#elif BUILDFLAG(IS_CHROMEOS)
// Sends a TouchEvent to the window system. |action| is a bitmask of the
// TouchType constants that indicates what events are generated, |id| identifies
// the touch point.
// TODO(mukai): consolidate this interface with the Windows SendTouchEvents.
bool SendTouchEvents(int action, int id, int x, int y);
bool SendTouchEventsNotifyWhenDone(int action,
                                   int id,
                                   int x,
                                   int y,
                                   base::OnceClosure task);
#endif

#if BUILDFLAG(IS_LINUX)
// Forces the platform implementation to use screen coordinates, even if they're
// not really available, the next time that ui_controls::SendMouseMove() or
// ui_controls::SendMouseMoveNotifyWhenDone() is called, or some other method
// using these methods internally, e.g. ui_test_utils::SendMouseMoveSync(). All
// following calls will behave normally (unless this method is called again).
void ForceUseScreenCoordinatesOnce();
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_WIN)
class UIControlsAura;
void InstallUIControlsAura(UIControlsAura* instance);
#endif

#if BUILDFLAG(IS_APPLE)
// Returns true when tests need to use extra Tab and Shift-Tab key events
// to traverse to the desired item; because the application is configured to
// traverse more elements for accessibility reasons.
bool IsFullKeyboardAccessEnabled();
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// TODO(vincentchiang): Move to another test API file.
// Update the test display configurations in accordance to the passed in
// |display_specs| which is a comma separated list of display specs. See
// ash::DisplayManagerTestApi::UpdateDisplay for detail.
void UpdateDisplaySync(const std::string& display_specs);
#endif

}  // namespace ui_controls

#endif  // UI_BASE_TEST_UI_CONTROLS_H_
