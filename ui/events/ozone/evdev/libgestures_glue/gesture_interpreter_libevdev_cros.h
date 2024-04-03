// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_GESTURE_INTERPRETER_LIBEVDEV_CROS_H_
#define UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_GESTURE_INTERPRETER_LIBEVDEV_CROS_H_

#include <gestures/gestures.h>
#include <libevdev/libevdev.h>

#include <bitset>
#include <memory>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "ui/events/ozone/evdev/cursor_delegate_evdev.h"
#include "ui/events/ozone/evdev/event_device_util.h"
#include "ui/events/ozone/evdev/event_dispatch_callback.h"
#include "ui/events/ozone/evdev/libgestures_glue/event_reader_libevdev_cros.h"

namespace ui {

class DeviceEventDispatcherEvdev;
class CursorDelegateEvdev;
struct GestureDeviceProperties;
class GesturePropertyProvider;

// Convert libevdev-cros events to ui::Events using libgestures.
//
// This builds a GestureInterpreter for an input device (touchpad or
// mouse).
//
// Raw input events must be preprocessed into a form suitable for
// libgestures. The kernel protocol only emits changes to the device state,
// so changes must be accumulated until a sync event. The full device state
// at sync is then processed by libgestures.
//
// Once we have the state at sync, we convert it to a HardwareState object
// and forward it to libgestures. If any gestures are produced, they are
// converted to ui::Events and dispatched.
class COMPONENT_EXPORT(EVDEV) GestureInterpreterLibevdevCros
    : public EventReaderLibevdevCros::Delegate {
 public:
  GestureInterpreterLibevdevCros(int id,
                                 CursorDelegateEvdev* cursor,
                                 GesturePropertyProvider* property_provider,
                                 DeviceEventDispatcherEvdev* dispatcher);

  GestureInterpreterLibevdevCros(const GestureInterpreterLibevdevCros&) =
      delete;
  GestureInterpreterLibevdevCros& operator=(
      const GestureInterpreterLibevdevCros&) = delete;

  ~GestureInterpreterLibevdevCros() override;

  // Overriden from ui::EventReaderLibevdevCros::Delegate
  void OnLibEvdevCrosOpen(Evdev* evdev, EventStateRec* evstate) override;
  void OnLibEvdevCrosEvent(Evdev* evdev,
                           EventStateRec* evstate,
                           const timeval& time) override;
  void OnLibEvdevCrosStopped(Evdev* evdev, EventStateRec* state) override;
  void SetupHapticButtonGeneration(
      const base::RepeatingCallback<void(bool)>& callback) override;
  void SetReceivedValidKeyboardInputCallback(
      base::RepeatingCallback<void(uint64_t)> callback) override;
  void SetReceivedValidMouseInputCallback(
      base::RepeatingCallback<void(int)> callback) override;
  void SetBlockModifiers(bool block_modifiers) override;

  // Handler for gesture events generated from libgestures.
  void OnGestureReady(const Gesture* gesture);

  // Accessors.
  int id() { return id_; }
  GesturePropertyProvider* property_provider() { return property_provider_; }
  Evdev* evdev() { return evdev_; }

 private:
  void OnGestureMove(const Gesture* gesture, const GestureMove* move);
  void OnGestureScroll(const Gesture* gesture, const GestureScroll* move);
  void OnGestureMouseWheel(const Gesture* gesture,
                           const GestureMouseWheel* wheel);
  void OnGestureButtonsChange(const Gesture* gesture,
                              const GestureButtonsChange* move);
  void OnGestureContactInitiated(const Gesture* gesture);
  void OnGestureFling(const Gesture* gesture, const GestureFling* fling);
  void OnGestureSwipe(const Gesture* gesture, const GestureSwipe* swipe);
  void OnGestureSwipeLift(const Gesture* gesture,
                          const GestureSwipeLift* swipelift);
  void OnGestureFourFingerSwipe(const Gesture* gesture,
                                const GestureFourFingerSwipe* swipe);
  void OnGestureFourFingerSwipeLift(const Gesture* gesture,
                                    const GestureFourFingerSwipeLift* swipe);
  void OnGesturePinch(const Gesture* gesture, const GesturePinch* pinch);
  void OnGestureMetrics(const Gesture* gesture, const GestureMetrics* metrics);

  void DispatchChangedMouseButtons(unsigned int changed_buttons,
                                   bool down,
                                   stime_t time);
  void DispatchMouseButton(unsigned int button,
                           bool down,
                           stime_t time);
  void DispatchChangedKeys(unsigned long* changed_keys, stime_t timestamp);
  void ReleaseKeys(stime_t timestamp);
  bool SetMouseButtonState(unsigned int button, bool down);
  void ReleaseMouseButtons(stime_t timestamp);
  void RecordClickMetric(stime_t duration, float movement);

  // The unique device id.
  int id_;

  // True if the device may be regarded as a mouse. This includes normal mice
  // and multi-touch mice.
  bool is_mouse_ = false;
  bool is_pointing_stick_ = false;

  // Whether modifier keys should be blocked from the input device.
  bool block_modifiers_;

  // Shared cursor state.
  CursorDelegateEvdev* cursor_;

  // Shared gesture property provider.
  GesturePropertyProvider* property_provider_;

  // Dispatcher for events.
  DeviceEventDispatcherEvdev* dispatcher_;

  // Gestures interpretation state.
  gestures::GestureInterpreter* interpreter_ = nullptr;

  // Last key state from libevdev.
  unsigned long prev_key_state_[EVDEV_BITS_TO_LONGS(KEY_CNT)];

  // Last mouse button state.
  static const int kMouseButtonCount = BTN_JOYSTICK - BTN_MOUSE;
  std::bitset<kMouseButtonCount> mouse_button_state_;

  stime_t click_down_time_;
  gfx::Vector2dF click_movement_;

  // Device pointer.
  Evdev* evdev_ = nullptr;

  // Gesture lib device properties.
  std::unique_ptr<GestureDeviceProperties> device_properties_;

  // The number of pixels to count as one "tick" on a multitouch mouse.
  static const int kMultitouchMousePixelsPerTick = 50;

  // Callback for physical button clicks.
  base::RepeatingCallback<void(bool)> click_callback_;

  // Callback for when a keyboard key press is registered.
  base::RepeatingCallback<void(uint64_t)> received_keyboard_input_;

  // Callback for when a mouse rel event is registered.
  base::RepeatingCallback<void(int)> received_mouse_input_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_GESTURE_INTERPRETER_LIBEVDEV_CROS_H_
