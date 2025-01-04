// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PUBLIC_STUB_INPUT_CONTROLLER_H_
#define UI_OZONE_PUBLIC_STUB_INPUT_CONTROLLER_H_

#include "base/component_export.h"
#include "ui/ozone/public/input_controller.h"

namespace ui {

// An InputController implementation that does nothing.
class COMPONENT_EXPORT(OZONE_BASE) StubInputController
    : public InputController {
 public:
  StubInputController() = default;

  StubInputController(const StubInputController&) = delete;
  StubInputController& operator=(const StubInputController&) = delete;

  ~StubInputController() override = default;

  // InputController:
  bool HasMouse() override;
  bool HasPointingStick() override;
  bool HasTouchpad() override;
  bool HasHapticTouchpad() override;
  bool IsCapsLockEnabled() override;
  void SetCapsLockEnabled(bool enabled) override;
  void SetNumLockEnabled(bool enabled) override;
  bool IsAutoRepeatEnabled() override;
  void SetAutoRepeatEnabled(bool enabled) override;
  void SetAutoRepeatRate(const base::TimeDelta& delay,
                         const base::TimeDelta& interval) override;
  void GetAutoRepeatRate(base::TimeDelta* delay,
                         base::TimeDelta* interval) override;
  void SetSlowKeysEnabled(bool enabled) override;
  bool IsSlowKeysEnabled() const override;
  void SetSlowKeysDelay(base::TimeDelta delay) override;
  void SetCurrentLayoutByName(const std::string& layout_name,
                              base::OnceCallback<void(bool)> callback) override;
  void SetKeyboardKeyBitsMapping(
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) override;
  std::vector<uint64_t> GetKeyboardKeyBits(int id) override;
  void SetTouchEventLoggingEnabled(bool enabled) override;
  void SuspendMouseAcceleration() override;
  void EndMouseAccelerationSuspension() override;
  void SetThreeFingerClick(bool enabled) override;
  void SetTouchpadSensitivity(std::optional<int> device_id, int value) override;
  void SetTouchpadScrollSensitivity(std::optional<int> device_id,
                                    int value) override;
  void SetTouchpadHapticFeedback(std::optional<int> device_id,
                                 bool enabled) override;
  void SetTouchpadHapticClickSensitivity(std::optional<int> device_id,
                                         int value) override;
  void SetTapToClick(std::optional<int> device_id, bool enabled) override;
  void SetTapDragging(std::optional<int> device_id, bool enabled) override;
  void SetNaturalScroll(std::optional<int> device_id, bool enabled) override;
  void SetMouseSensitivity(std::optional<int> device_id, int value) override;
  void SetMouseScrollSensitivity(std::optional<int> device_id,
                                 int value) override;
  void SetMouseReverseScroll(std::optional<int> device_id,
                             bool enabled) override;
  void SetMouseAcceleration(std::optional<int> device_id,
                            bool enabled) override;
  void SetMouseScrollAcceleration(std::optional<int> device_id,
                                  bool enabled) override;
  void SetPointingStickSensitivity(std::optional<int> device_id,
                                   int value) override;
  void SetPointingStickAcceleration(std::optional<int> device_id,
                                    bool enabled) override;
  void SetTouchpadAcceleration(std::optional<int> device_id,
                               bool enabled) override;
  void SetTouchpadScrollAcceleration(std::optional<int> device_id,
                                     bool enabled) override;
  void SetPrimaryButtonRight(std::optional<int> device_id, bool right) override;
  void SetPointingStickPrimaryButtonRight(std::optional<int> device_id,
                                          bool right) override;
  void SetGamepadKeyBitsMapping(
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) override;
  std::vector<uint64_t> GetGamepadKeyBits(int id) override;
  void SetTapToClickPaused(bool state) override;
  void GetTouchDeviceStatus(GetTouchDeviceStatusReply reply) override;
  void GetTouchEventLog(const base::FilePath& out_dir,
                        GetTouchEventLogReply reply) override;
  void DescribeForLog(DescribeForLogReply reply) const override;
  void SetInternalTouchpadEnabled(bool enabled) override;
  bool IsInternalTouchpadEnabled() const override;
  void SetTouchscreensEnabled(bool enabled) override;
  void GetStylusSwitchState(GetStylusSwitchStateReply reply) override;
  void SetInternalKeyboardFilter(bool enable_filter,
                                 std::vector<DomCode> allowed_keys) override;
  void GetGesturePropertiesService(
      mojo::PendingReceiver<ui::ozone::mojom::GesturePropertiesService>
          receiver) override;
  void PlayVibrationEffect(int id,
                           uint8_t amplitude,
                           uint16_t duration_millis) override;
  void StopVibration(int id) override;
  void PlayHapticTouchpadEffect(HapticTouchpadEffect effect_type,
                                HapticTouchpadEffectStrength strength) override;
  void SetHapticTouchpadEffectForNextButtonRelease(
      HapticTouchpadEffect effect_type,
      HapticTouchpadEffectStrength strength) override;
  bool AreAnyKeysPressed() override;
  void BlockModifiersOnDevices(std::vector<int> device_ids) override;

  bool AreInputDevicesEnabled() const override;
  std::unique_ptr<ScopedDisableInputDevices> DisableInputDevices() override;

  void DisableKeyboardImposterCheck() override;

 private:
  friend class ScopedDisableInputDevicesImpl;

  int num_scoped_input_devices_disablers_ = 0;
};

}  // namespace ui

#endif  // UI_OZONE_PUBLIC_STUB_INPUT_CONTROLLER_H_
