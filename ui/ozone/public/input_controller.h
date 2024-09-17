// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_INPUT_CONTROLLER_H_
#define UI_OZONE_PUBLIC_INPUT_CONTROLLER_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/ozone/public/mojom/gesture_properties_service.mojom.h"

namespace base {
class TimeDelta;
}

namespace ui {
enum class StylusState;
enum class HapticTouchpadEffect;
enum class HapticTouchpadEffectStrength;
}  // namespace ui

namespace ui {

enum class DomCode : uint32_t;

// Disables all input devices until it is destroyed.
// You should not create this yourself but instead get it from
// `InputController::DisableInputDevices()`.
class ScopedDisableInputDevices {
 public:
  virtual ~ScopedDisableInputDevices() = default;

 protected:
  ScopedDisableInputDevices() = default;
};

// Platform-specific interface for controlling input devices.
//
// The object provides methods for the preference page to configure input
// devices w.r.t. the user setting. On ChromeOS, this replaces the inputcontrol
// script that is originally located at /opt/google/chrome/.
class COMPONENT_EXPORT(OZONE_BASE) InputController {
 public:
  using GetTouchDeviceStatusReply =
      base::OnceCallback<void(const std::string&)>;
  // TODO(sky): convert this to value once mojo supports move for vectors.
  using GetTouchEventLogReply =
      base::OnceCallback<void(const std::vector<base::FilePath>&)>;
  using GetStylusSwitchStateReply = base::OnceCallback<void(ui::StylusState)>;
  using DescribeForLogReply = base::OnceCallback<void(const std::string&)>;

  InputController() = default;

  InputController(const InputController&) = delete;
  InputController& operator=(const InputController&) = delete;

  virtual ~InputController() = default;

  // Functions for checking devices existence.
  virtual bool HasMouse() = 0;
  virtual bool HasPointingStick() = 0;
  virtual bool HasTouchpad() = 0;
  virtual bool HasHapticTouchpad() = 0;

  // Keyboard settings.
  virtual bool IsCapsLockEnabled() = 0;
  virtual void SetCapsLockEnabled(bool enabled) = 0;
  virtual void SetNumLockEnabled(bool enabled) = 0;
  virtual bool IsAutoRepeatEnabled() = 0;
  virtual void SetAutoRepeatEnabled(bool enabled) = 0;
  virtual void SetAutoRepeatRate(const base::TimeDelta& delay,
                                 const base::TimeDelta& interval) = 0;
  virtual void GetAutoRepeatRate(base::TimeDelta* delay,
                                 base::TimeDelta* interval) = 0;
  // Callback is invoked when the keyboard layout is available and initialized.
  virtual void SetCurrentLayoutByName(
      const std::string& layout_name,
      base::OnceCallback<void(bool)> callback) = 0;
  virtual void SetKeyboardKeyBitsMapping(
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) = 0;
  virtual std::vector<uint64_t> GetKeyboardKeyBits(int id) = 0;

  // Touchpad settings.
  // If `nullopt` is passed instead of a `device_id`, settings will be applied
  // to all touchpads instead of per-device.
  virtual void SetThreeFingerClick(bool enabled) = 0;
  virtual void SetTouchpadSensitivity(std::optional<int> device_id,
                                      int value) = 0;
  virtual void SetTouchpadScrollSensitivity(std::optional<int> device_id,
                                            int value) = 0;
  virtual void SetTapToClick(std::optional<int> device_id, bool enabled) = 0;
  virtual void SetTapDragging(std::optional<int> device_id, bool enabled) = 0;
  virtual void SetNaturalScroll(std::optional<int> device_id, bool enabled) = 0;
  virtual void SetTouchpadAcceleration(std::optional<int> device_id,
                                       bool enabled) = 0;
  virtual void SetTouchpadScrollAcceleration(std::optional<int> device_id,
                                             bool enabled) = 0;
  virtual void SetTouchpadHapticFeedback(std::optional<int> device_id,
                                         bool enabled) = 0;
  virtual void SetTouchpadHapticClickSensitivity(std::optional<int> device_id,
                                                 int value) = 0;

  // Mouse settings.
  // If `nullopt` is passed instead of a `device_id`, settings will be applied
  // to all mice instead of per-device.
  virtual void SetMouseSensitivity(std::optional<int> device_id, int value) = 0;
  virtual void SetMouseScrollSensitivity(std::optional<int> device_id,
                                         int value) = 0;

  // Sets the primary button for the mouse. Passing true sets the right button
  // as primary, while false (the default) sets the left as primary.
  virtual void SetPrimaryButtonRight(std::optional<int> device_id,
                                     bool right) = 0;
  virtual void SetMouseReverseScroll(std::optional<int> device_id,
                                     bool enabled) = 0;
  virtual void SetMouseAcceleration(std::optional<int> device_id,
                                    bool enabled) = 0;
  virtual void SetMouseScrollAcceleration(std::optional<int> device_id,
                                          bool enabled) = 0;
  virtual void SuspendMouseAcceleration() = 0;
  virtual void EndMouseAccelerationSuspension() = 0;

  // Pointing stick settings.
  // If `nullopt` is passed instead of a `device_id`, settings will be applied
  // to all pointing sticks instead of per-device.
  virtual void SetPointingStickSensitivity(std::optional<int> device_id,
                                           int value) = 0;

  // Sets the primary button for the pointing stick. Passing true sets the right
  // button as primary, while false (the default) sets the left as primary.
  virtual void SetPointingStickPrimaryButtonRight(std::optional<int> device_id,
                                                  bool right) = 0;
  virtual void SetPointingStickAcceleration(std::optional<int> device_id,
                                            bool enabled) = 0;

  // Gamepad settings.
  virtual void SetGamepadKeyBitsMapping(
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) = 0;
  virtual std::vector<uint64_t> GetGamepadKeyBits(int id) = 0;

  // Touch log collection.
  virtual void GetTouchDeviceStatus(GetTouchDeviceStatusReply reply) = 0;
  virtual void GetTouchEventLog(const base::FilePath& out_dir,
                                GetTouchEventLogReply reply) = 0;
  // Touchscreen log settings.
  virtual void SetTouchEventLoggingEnabled(bool enabled) = 0;

  // Describe internal state for system log.
  virtual void DescribeForLog(DescribeForLogReply) const = 0;

  // Temporarily enable/disable Tap-to-click. Used to enhance the user
  // experience in some use cases (e.g., typing, watching video).
  virtual void SetTapToClickPaused(bool state) = 0;

  virtual void SetInternalTouchpadEnabled(bool enabled) = 0;
  virtual bool IsInternalTouchpadEnabled() const = 0;

  virtual void SetTouchscreensEnabled(bool enabled) = 0;

  // Find out whether stylus is in its garage; may trigger callback
  // immediately on platforms where this cannot exist, otherwise
  // this is be an async reply.
  virtual void GetStylusSwitchState(GetStylusSwitchStateReply reply) = 0;

  // Controls vibration for the gamepad device with the corresponding |id|.
  // |amplitude| determines the strength of the vibration, where 0 is no
  // vibration and 255 is maximum vibration, and |duration_millis|
  // determines the duration of the vibration in milliseconds.
  virtual void PlayVibrationEffect(int id,
                                   uint8_t amplitude,
                                   uint16_t duration_millis) = 0;
  virtual void StopVibration(int id) = 0;

  // Control haptic feedback for haptic-capable touchpad devices.
  virtual void PlayHapticTouchpadEffect(
      HapticTouchpadEffect effect,
      HapticTouchpadEffectStrength strength) = 0;
  virtual void SetHapticTouchpadEffectForNextButtonRelease(
      HapticTouchpadEffect effect,
      HapticTouchpadEffectStrength strength) = 0;

  // If |enable_filter| is true, all keys on the internal keyboard except
  // |allowed_keys| are disabled.
  virtual void SetInternalKeyboardFilter(bool enable_filter,
                                         std::vector<DomCode> allowed_keys) = 0;

  virtual void GetGesturePropertiesService(
      mojo::PendingReceiver<ui::ozone::mojom::GesturePropertiesService>
          receiver) = 0;

  virtual bool AreAnyKeysPressed() = 0;

  // Blocks all modifiers from being emitted on devices with the given device
  // ids.
  virtual void BlockModifiersOnDevices(std::vector<int> device_ids) = 0;

  // Disable all input devices until the returned object is destroyed.
  // When called multiple times the input devices remain disabled until all
  // scoped return values are destroyed.
  [[nodiscard]] virtual std::unique_ptr<ScopedDisableInputDevices>
  DisableInputDevices() = 0;

  virtual bool AreInputDevicesEnabled() const = 0;

  // Disable imposter check when a keyboard is connected to the device. When a
  // new keyboard is connected to the device it is checked for being an
  // imposter which which triggers the device to use a virtual keyboard when
  // input is required until a physical key is pressed on the physical keyboard
  // connected.
  virtual void DisableKeyboardImposterCheck() = 0;
};

// Create an input controller that does nothing.
COMPONENT_EXPORT(OZONE_BASE)
std::unique_ptr<InputController> CreateStubInputController();

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_INPUT_CONTROLLER_H_
