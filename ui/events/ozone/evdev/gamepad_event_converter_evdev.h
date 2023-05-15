// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_GAMEPAD_EVENT_CONVERTER_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_GAMEPAD_EVENT_CONVERTER_EVDEV_H_

#include <ostream>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/event.h"
#include "ui/events/ozone/evdev/event_converter_evdev.h"
#include "ui/events/ozone/evdev/event_device_info.h"

struct input_event;

namespace ui {

class DeviceEventDispatcherEvdev;

class COMPONENT_EXPORT(EVDEV) GamepadEventConverterEvdev
    : public EventConverterEvdev {
 public:
  GamepadEventConverterEvdev(base::ScopedFD fd,
                             base::FilePath path,
                             int id,
                             const EventDeviceInfo& info,
                             DeviceEventDispatcherEvdev* dispatcher);

  GamepadEventConverterEvdev(const GamepadEventConverterEvdev&) = delete;
  GamepadEventConverterEvdev& operator=(const GamepadEventConverterEvdev&) =
      delete;

  ~GamepadEventConverterEvdev() override;

  // EventConverterEvdev:
  void OnFileCanReadWithoutBlocking(int fd) override;
  bool HasGamepad() const override;
  void OnDisabled() override;
  std::vector<ui::GamepadDevice::Axis> GetGamepadAxes() const override;
  bool GetGamepadRumbleCapability() const override;
  std::vector<uint64_t> GetGamepadKeyBits() const override;

  // This function processes one input_event from evdev.
  void ProcessEvent(const struct input_event& input);

  // This function sends a vibration effect to the gamepaddevice.
  void PlayVibrationEffect(uint8_t amplitude,
                           uint16_t duration_millis) override;

  // This function stops the gamepad device's vibration effect.
  void StopVibration() override;

  std::ostream& DescribeForLog(std::ostream& os) const override;

 private:
  // This function processes EV_KEY event from gamepad device.
  void ProcessEvdevKey(uint16_t code,
                       int value,
                       const base::TimeTicks& timestamp);

  // This function processes EV_ABS event from gamepad device.
  void ProcessEvdevAbs(uint16_t code,
                       int value,
                       const base::TimeTicks& timestamp);

  // This function releases all the keys and resets all the axises.
  void ResetGamepad();

  // This function reads current gamepad status and resyncs the gamepad.
  void ResyncGamepad();

  void OnSync(const base::TimeTicks& timestamp);

  // This function uploads the rumble force feedback effect to the gamepad
  // device and returns the new effect id. If we already created an effect on
  // this device, then the existing id is reused and returned.
  int StoreRumbleEffect(const base::ScopedFD& fd,
                        int effect_id,
                        uint16_t duration,
                        uint16_t start_delay,
                        uint16_t strong_magnitude,
                        uint16_t weak_magnitude);

  // This function controls the playback of the effect on the gamepad device.
  void StartOrStopEffect(const base::ScopedFD& fd,
                         int effect_id,
                         bool do_start);

  // This function removes the effect from the gamepad device.
  void DestroyEffect(const base::ScopedFD& fd, int effect_id);

  // This function writes the input_event into the kernel and returns the result
  // of the write.
  virtual ssize_t WriteEvent(const base::ScopedFD& fd,
                             const struct input_event& input);

  // This function uploads the ff_effect to the gamepad device and returns the
  // unique id assigned by the driver.
  virtual int UploadFfEffect(const base::ScopedFD& fd,
                             struct ff_effect* effect);

  // Sometimes, we want to drop abs values, when we do so, we no longer want to
  // send gamepad frame event when we see next sync. This flag is set to false
  // when each frame is sent. It is set to true when Btn or Abs event is sent.
  bool will_send_frame_;

  // This flag is set to true if the gamepad supports force feedback of type
  // FF_RUMBLE.
  bool supports_rumble_;

  std::vector<ui::GamepadDevice::Axis> axes_;

  // Evdev scancodes of pressed buttons.
  base::flat_set<unsigned int> pressed_buttons_;

  // Input device file descriptor.
  const base::ScopedFD input_device_fd_;

  // Callbacks for dispatching events.
  const raw_ptr<DeviceEventDispatcherEvdev> dispatcher_;

  // The effect id is needed to keep track of effects that are uploaded and
  // stored in the gamepad device.
  int effect_id_;

  std::vector<uint64_t> key_bits_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_GAMEPAD_EVENT_CONVERTER_EVDEV_H_
