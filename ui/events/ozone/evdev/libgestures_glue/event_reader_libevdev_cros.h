// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_EVENT_READER_LIBEVDEV_CROS_H_
#define UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_EVENT_READER_LIBEVDEV_CROS_H_

#include <libevdev/libevdev.h>

#include <memory>
#include <ostream>

#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "ui/events/ozone/evdev/event_converter_evdev.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/input_device_settings_evdev.h"
#include "ui/events/ozone/evdev/libgestures_glue/haptic_touchpad_handler.h"

namespace ui {

// Basic wrapper for libevdev-cros.
//
// This drives libevdev-cros from a file descriptor and calls delegate
// with the updated event state from libevdev-cros.
//
// The library doesn't support all devices currently. In particular there
// is no support for keyboard events.
class EventReaderLibevdevCros : public EventConverterEvdev {
 public:
  class Delegate {
   public:
    virtual ~Delegate();

    // Notifier for open. This is called with the initial event state.
    virtual void OnLibEvdevCrosOpen(Evdev* evdev, EventStateRec* evstate) = 0;

    // Notifier for event. This is called with the updated event state.
    virtual void OnLibEvdevCrosEvent(Evdev* evdev,
                                     EventStateRec* state,
                                     const timeval& time) = 0;

    // Notifier for stop. This is called with the final event state.
    virtual void OnLibEvdevCrosStopped(Evdev* evdev, EventStateRec* state) = 0;

    // For haptic touchpads. Start using gesture library to determine when
    // physical clicks occur, based on force thresholds.
    // The passed callback function should be called whenever the gesture
    // library determines that a physical click has occurred.
    virtual void SetupHapticButtonGeneration(
        const base::RepeatingCallback<void(bool)>& callback) = 0;

    // For keyboard/pointer combo devices. Sets a callback that will be called
    // whenever the device registers valid keyboard input.
    virtual void SetReceivedValidKeyboardInputCallback(
        base::RepeatingCallback<void(uint64_t)> callback) = 0;

    // For keyboard/pointer combo devices. Sets a callback that will be called
    // whenever the device registers valid mouse input.
    virtual void SetReceivedValidMouseInputCallback(
        base::RepeatingCallback<void(int)> callback) = 0;

    // Sets whether modifier events should be blocked when coming from this
    // device.
    virtual void SetBlockModifiers(bool block_modifiers) = 0;
  };

  EventReaderLibevdevCros(base::ScopedFD fd,
                          const base::FilePath& path,
                          int id,
                          const EventDeviceInfo& devinfo,
                          std::unique_ptr<Delegate> delegate);

  EventReaderLibevdevCros(const EventReaderLibevdevCros&) = delete;
  EventReaderLibevdevCros& operator=(const EventReaderLibevdevCros&) = delete;

  ~EventReaderLibevdevCros() override;

  // Used as a callback for `Delegate` to call when the device registers valid
  // keyboard input.
  void ReceivedKeyboardInput(uint64_t key);

  // Used as a callback for `Delegate` to call when the device registers valid
  // mouse input.
  void ReceivedMouseInput(int rel_value);

  // EventConverterEvdev:
  void OnFileCanReadWithoutBlocking(int fd) override;
  bool HasKeyboard() const override;
  bool HasMouse() const override;
  bool HasPointingStick() const override;
  bool HasTouchpad() const override;
  bool HasHapticTouchpad() const override;
  bool HasCapsLockLed() const override;
  bool HasStylusSwitch() const override;
  void OnDisabled() override;
  void PlayHapticTouchpadEffect(HapticTouchpadEffect effect,
                                HapticTouchpadEffectStrength strength) override;
  void SetHapticTouchpadEffectForNextButtonRelease(
      HapticTouchpadEffect effect,
      HapticTouchpadEffectStrength strength) override;
  void ApplyDeviceSettings(const InputDeviceSettingsEvdev& settings) override;
  void SetReceivedValidInputCallback(
      ReceivedValidInputCallback callback) override;
  void SetBlockModifiers(bool block_modifiers) override;

  std::ostream& DescribeForLog(std::ostream& os) const override;

 private:
  static void OnSynReport(void* data,
                          EventStateRec* evstate,
                          struct timeval* tv);
  static void OnLogMessage(void*, int level, const char*, ...);

  // Returns true if this is a haptic touchpad, UI haptics are enabled, and it
  // is actively being touched.
  bool CanHandleHapticFeedback() const;

  // Input modalities for this device.
  bool has_keyboard_;
  bool has_mouse_;
  bool has_pointing_stick_;
  bool has_touchpad_;
  bool has_stylus_switch_;

  // LEDs for this device.
  bool has_caps_lock_led_;

  // Libevdev state.
  Evdev evdev_;

  // Event state.
  EventStateRec evstate_;

  // Path to input device.
  base::FilePath path_;

  // For touchpads, number of fingers present.
  int touch_count_;

  // Is UI haptic feedback enabled?
  bool haptic_feedback_enabled_;

  // Delegate for event processing.
  std::unique_ptr<Delegate> delegate_;

  // Haptic effect handling for touchpads
  std::unique_ptr<HapticTouchpadHandler> haptic_touchpad_handler_;

  // Callback to update keyboard devices when valid input is received.
  ReceivedValidInputCallback received_valid_input_callback_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_LIBGESTURES_GLUE_EVENT_READER_LIBEVDEV_CROS_H_
