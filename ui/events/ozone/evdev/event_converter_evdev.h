// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_EVDEV_H_
#define UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_EVDEV_H_

#include <stdint.h>

#include <ostream>
#include <set>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/task/current_thread.h"
#include "ui/events/devices/gamepad_device.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/event_dispatch_callback.h"
#include "ui/events/ozone/evdev/touch_evdev_types.h"
#include "ui/gfx/geometry/size.h"

struct input_event;

namespace ui {
enum class DomCode : uint32_t;
struct InputDeviceSettingsEvdev;

class COMPONENT_EXPORT(EVDEV) EventConverterEvdev
    : public base::MessagePumpForUI::FdWatcher {
 public:
  using ReportStylusStateCallback =
      base::RepeatingCallback<void(const InProgressTouchEvdev&,
                                   const int32_t x_res,
                                   const int32_t y_res,
                                   const base::TimeTicks&)>;

  using GetLatestStylusStateCallback =
      base::RepeatingCallback<void(const InProgressStylusState**)>;

  using ReceivedValidInputCallback =
      base::RepeatingCallback<void(const EventConverterEvdev* converter)>;

  EventConverterEvdev(int fd,
                      const base::FilePath& path,
                      int id,
                      InputDeviceType type,
                      const std::string& name,
                      const std::string& phys,
                      uint16_t vendor_id,
                      uint16_t product_id,
                      uint16_t version);

  EventConverterEvdev(const EventConverterEvdev&) = delete;
  EventConverterEvdev& operator=(const EventConverterEvdev&) = delete;

  ~EventConverterEvdev() override;

  int id() const { return input_device_.id; }

  const base::FilePath& path() const { return path_; }

  InputDeviceType type() const { return input_device_.type; }

  const InputDevice& input_device() const { return input_device_; }

  // Update device settings. The default implementation doesn't do
  // anything
  virtual void ApplyDeviceSettings(const InputDeviceSettingsEvdev& settings);

  // Start reading events.
  void Start();

  // Stop reading events.
  void Stop();

  // Enable or disable this device. A disabled device still polls for
  // input and can update state but must not dispatch any events to UI.
  void SetEnabled(bool enabled);

  bool IsEnabled() const;

  // Flag this device as being suspected for falsely identifying as a keyboard.
  void SetSuspectedKeyboardImposter(bool is_suspected);

  bool IsSuspectedKeyboardImposter() const;

  // Flag this device as being suspected for falsely identifying as a mouse.
  void SetSuspectedMouseImposter(bool is_suspected);

  bool IsSuspectedMouseImposter() const;

  // Cleanup after we stop reading events (release buttons, etc).
  virtual void OnStopped();

  // Prepare for disable (e.g. should release keys/buttons/touches).
  virtual void OnDisabled();

  // Start or restart (e.g. should reapply keys/buttons/touches).
  virtual void OnEnabled();

  // Dump recent events into a file.
  virtual void DumpTouchEventLog(const char* filename);

  // Returns value corresponding to keyboard status (No Keyboard, Keyboard in
  // Blocklist, ect.).
  virtual KeyboardType GetKeyboardType() const;

  // Returns true if the converter is used for a keyboard device.
  virtual bool HasKeyboard() const;

  // Returns true if the converter is used for a mouse device (that isn't a
  // pointing stick);
  virtual bool HasMouse() const;

  // Returns true if the converter is used for a pointing stick device (such as
  // a TrackPoint);
  virtual bool HasPointingStick() const;

  // Returns true if the converter is used for a touchpad device.
  virtual bool HasTouchpad() const;

  // Returns true if the converter is used for a haptic touchpad device.
  // If HasHapticTouchpad() is true, then HasTouchpad() is also true.
  virtual bool HasHapticTouchpad() const;

  // Returns true if the converter is used for a touchscreen device.
  virtual bool HasTouchscreen() const;

  // Returns true if the converter is used for a pen device.
  virtual bool HasPen() const;

  // Returns true if the converter is used for a device with gamepad input.
  virtual bool HasGamepad() const;

  // Returns true if the converter is used for a device with graphics tablet
  // input.
  virtual bool HasGraphicsTablet() const;

  // Returns true if the converter is used for a device with a caps lock LED.
  virtual bool HasCapsLockLed() const;

  // Returns true if the converter is used for a device with a stylus switch
  // (also known as garage or dock sensor, not buttons on a stylus).
  virtual bool HasStylusSwitch() const;

  // Returns true if the converter is a keyboard and has an assistant key.
  virtual bool HasAssistantKey() const;

  // Returns true if the converter is a keyboard and has a function key.
  virtual bool HasFunctionKey() const;

  // Returns the current state of the stylus garage switch, indicating whether a
  // stylus is inserted in (or attached) to a stylus dock or garage, or has been
  // removed.
  virtual ui::StylusState GetStylusSwitchState();

  // Returns the size of the touchscreen device if the converter is used for a
  // touchscreen device.
  virtual gfx::Size GetTouchscreenSize() const;

  // Returns the number of touch points this device supports. Should not be
  // called unless HasTouchscreen() returns true
  virtual int GetTouchPoints() const;

  // Returns information for all axes if the converter is used for a gamepad
  // device.
  virtual std::vector<GamepadDevice::Axis> GetGamepadAxes() const;

  // Returns whether the gamepad device supports rumble type force feedback.
  virtual bool GetGamepadRumbleCapability() const;

  // Returns supported key bits of the gamepad.
  virtual std::vector<uint64_t> GetGamepadKeyBits() const;

  // Sets which keyboard keys should be processed. If |enable_filter| is
  // false, all keys are allowed and |allowed_keys| is ignored.
  virtual void SetKeyFilter(bool enable_filter,
                            std::vector<DomCode> allowed_keys);

  // Set that modifier keys should not be allowed to be produced from this
  // converter.
  virtual void SetBlockModifiers(bool block_modifiers);

  // Update caps lock LED state.
  virtual void SetCapsLockLed(bool enabled);

  // Update touch event logging state.
  virtual void SetTouchEventLoggingEnabled(bool enabled);

  // Sets callback to enable/disable palm suppression.
  virtual void SetPalmSuppressionCallback(
      const base::RepeatingCallback<void(bool)>& callback);

  // Sets callback to report the latest stylus state.
  virtual void SetReportStylusStateCallback(
      const ReportStylusStateCallback& callback);

  // Sets callback to get the latest stylus state.
  virtual void SetGetLatestStylusStateCallback(
      const GetLatestStylusStateCallback& callback);

  // Set callback to trigger keyboard device update.
  virtual void SetReceivedValidInputCallback(
      ReceivedValidInputCallback callback);

  // Returns supported key bits of the keyboard.
  virtual std::vector<uint64_t> GetKeyboardKeyBits() const;

  // Helper to generate a base::TimeTicks from an input_event's time
  static base::TimeTicks TimeTicksFromInputEvent(const input_event& event);

  static bool IsValidKeyboardKeyPress(uint64_t key);

  // Handle gamepad force feedback effects.
  virtual void PlayVibrationEffect(uint8_t amplitude, uint16_t duration_millis);
  virtual void StopVibration();

  // Handle haptic touchpad effects.
  virtual void PlayHapticTouchpadEffect(HapticTouchpadEffect effect,
                                        HapticTouchpadEffectStrength strength);
  virtual void SetHapticTouchpadEffectForNextButtonRelease(
      HapticTouchpadEffect effect,
      HapticTouchpadEffectStrength strength);

  // Describe converter for system log.
  virtual std::ostream& DescribeForLog(std::ostream& os) const;

 protected:
  // base::MessagePumpForUI::FdWatcher:
  void OnFileCanWriteWithoutBlocking(int fd) override;

  // File descriptor to read.
  const int fd_;

  // Path to input device.
  const base::FilePath path_;

  // Input device information, including id (which uniquely identifies an
  // event converter) and type.
  InputDevice input_device_;

  // Whether we're polling for input on the device.
  bool watching_ = false;

  // Controller for watching the input fd.
  base::MessagePumpForUI::FdWatchController controller_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_EVENT_CONVERTER_EVDEV_H_
