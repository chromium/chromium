// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/stub_input_controller.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "ui/events/devices/stylus_state.h"

namespace ui {

class ScopedDisableInputDevicesImpl : public ScopedDisableInputDevices {
 public:
  explicit ScopedDisableInputDevicesImpl(StubInputController& parent)
      : parent_(parent) {
    parent_->num_scoped_input_devices_disablers_++;
  }

  ~ScopedDisableInputDevicesImpl() override {
    parent_->num_scoped_input_devices_disablers_--;
  }

 private:
  raw_ref<StubInputController> parent_;
};

bool StubInputController::HasMouse() {
  return false;
}
bool StubInputController::HasPointingStick() {
  return false;
}
bool StubInputController::HasTouchpad() {
  return false;
}
bool StubInputController::HasHapticTouchpad() {
  return false;
}
bool StubInputController::IsCapsLockEnabled() {
  return false;
}
void StubInputController::SetCapsLockEnabled(bool enabled) {}
void StubInputController::SetNumLockEnabled(bool enabled) {}
bool StubInputController::IsAutoRepeatEnabled() {
  return true;
}
void StubInputController::SetAutoRepeatEnabled(bool enabled) {}
void StubInputController::SetAutoRepeatRate(const base::TimeDelta& delay,
                                            const base::TimeDelta& interval) {}
void StubInputController::GetAutoRepeatRate(base::TimeDelta* delay,
                                            base::TimeDelta* interval) {}
void StubInputController::SetSlowKeysEnabled(bool enabled) {}
bool StubInputController::IsSlowKeysEnabled() const {
  return false;
}
void StubInputController::SetSlowKeysDelay(base::TimeDelta delay) {}
void StubInputController::SetCurrentLayoutByName(
    const std::string& layout_name,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(false);
}
void StubInputController::SetKeyboardKeyBitsMapping(
    base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) {}
std::vector<uint64_t> StubInputController::GetKeyboardKeyBits(int id) {
  return std::vector<uint64_t>();
}
void StubInputController::SetTouchEventLoggingEnabled(bool enabled) {
  NOTIMPLEMENTED_LOG_ONCE();
}
void StubInputController::SuspendMouseAcceleration() {}
void StubInputController::EndMouseAccelerationSuspension() {}
void StubInputController::SetThreeFingerClick(bool enabled) {}
void StubInputController::SetTouchpadSensitivity(std::optional<int> device_id,
                                                 int value) {}
void StubInputController::SetTouchpadScrollSensitivity(
    std::optional<int> device_id,
    int value) {}
void StubInputController::SetTouchpadHapticFeedback(
    std::optional<int> device_id,
    bool enabled) {}
void StubInputController::SetTouchpadHapticClickSensitivity(
    std::optional<int> device_id,
    int value) {}
void StubInputController::SetTapToClick(std::optional<int> device_id,
                                        bool enabled) {}
void StubInputController::SetTapDragging(std::optional<int> device_id,
                                         bool enabled) {}
void StubInputController::SetNaturalScroll(std::optional<int> device_id,
                                           bool enabled) {}
void StubInputController::SetMouseSensitivity(std::optional<int> device_id,
                                              int value) {}
void StubInputController::SetMouseScrollSensitivity(
    std::optional<int> device_id,
    int value) {}
void StubInputController::SetMouseReverseScroll(std::optional<int> device_id,
                                                bool enabled) {}
void StubInputController::SetMouseAcceleration(std::optional<int> device_id,
                                               bool enabled) {}
void StubInputController::SetMouseScrollAcceleration(
    std::optional<int> device_id,
    bool enabled) {}
void StubInputController::SetPointingStickSensitivity(
    std::optional<int> device_id,
    int value) {}
void StubInputController::SetPointingStickAcceleration(
    std::optional<int> device_id,
    bool enabled) {}
void StubInputController::SetTouchpadAcceleration(std::optional<int> device_id,
                                                  bool enabled) {}
void StubInputController::SetTouchpadScrollAcceleration(
    std::optional<int> device_id,
    bool enabled) {}
void StubInputController::SetPrimaryButtonRight(std::optional<int> device_id,
                                                bool right) {}
void StubInputController::SetPointingStickPrimaryButtonRight(
    std::optional<int> device_id,
    bool right) {}
void StubInputController::SetGamepadKeyBitsMapping(
    base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) {}
std::vector<uint64_t> StubInputController::GetGamepadKeyBits(int id) {
  return std::vector<uint64_t>();
}
void StubInputController::SetTapToClickPaused(bool state) {}
void StubInputController::GetTouchDeviceStatus(
    GetTouchDeviceStatusReply reply) {
  std::move(reply).Run(std::string());
}
void StubInputController::GetTouchEventLog(const base::FilePath& out_dir,
                                           GetTouchEventLogReply reply) {
  std::move(reply).Run(std::vector<base::FilePath>());
}
void StubInputController::DescribeForLog(DescribeForLogReply reply) const {
  std::move(reply).Run(std::string());
}
void StubInputController::SetInternalTouchpadEnabled(bool enabled) {}
bool StubInputController::IsInternalTouchpadEnabled() const {
  return false;
}
void StubInputController::SetTouchscreensEnabled(bool enabled) {}
void StubInputController::GetStylusSwitchState(
    GetStylusSwitchStateReply reply) {
  std::move(reply).Run(ui::StylusState::REMOVED);
}
void StubInputController::SetInternalKeyboardFilter(
    bool enable_filter,
    std::vector<DomCode> allowed_keys) {}
void StubInputController::GetGesturePropertiesService(
    mojo::PendingReceiver<ui::ozone::mojom::GesturePropertiesService>
        receiver) {}
void StubInputController::PlayVibrationEffect(int id,
                                              uint8_t amplitude,
                                              uint16_t duration_millis) {}
void StubInputController::StopVibration(int id) {}
void StubInputController::PlayHapticTouchpadEffect(
    HapticTouchpadEffect effect_type,
    HapticTouchpadEffectStrength strength) {}
void StubInputController::SetHapticTouchpadEffectForNextButtonRelease(
    HapticTouchpadEffect effect_type,
    HapticTouchpadEffectStrength strength) {}
bool StubInputController::AreAnyKeysPressed() {
  return false;
}
void StubInputController::BlockModifiersOnDevices(std::vector<int> device_ids) {
}

bool StubInputController::AreInputDevicesEnabled() const {
  return num_scoped_input_devices_disablers_ == 0;
}
std::unique_ptr<ScopedDisableInputDevices>
StubInputController::DisableInputDevices() {
  return std::make_unique<ScopedDisableInputDevicesImpl>(*this);
}

void StubInputController::DisableKeyboardImposterCheck() {}

}  // namespace ui
