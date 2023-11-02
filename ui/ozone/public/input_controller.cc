// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/input_controller.h"

#include <memory>

#include "base/callback.h"
#include "base/logging.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/devices/stylus_state.h"

namespace ui {

namespace {

class StubInputController : public InputController {
 public:
  StubInputController() = default;

  StubInputController(const StubInputController&) = delete;
  StubInputController& operator=(const StubInputController&) = delete;

  ~StubInputController() override = default;

  // InputController:
  bool HasMouse() override { return false; }
  bool HasPointingStick() override { return false; }
  bool HasTouchpad() override { return false; }
  bool HasHapticTouchpad() override { return false; }
  bool IsCapsLockEnabled() override { return false; }
  void SetCapsLockEnabled(bool enabled) override {}
  void SetNumLockEnabled(bool enabled) override {}
  bool IsAutoRepeatEnabled() override { return true; }
  void SetAutoRepeatEnabled(bool enabled) override {}
  void SetAutoRepeatRate(const base::TimeDelta& delay,
                         const base::TimeDelta& interval) override {}
  void GetAutoRepeatRate(base::TimeDelta* delay,
                         base::TimeDelta* interval) override {}
  void SetCurrentLayoutByName(const std::string& layout_name) override {}
  void SetKeyboardKeyBitsMapping(
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) override {}
  std::vector<uint64_t> GetKeyboardKeyBits(int id) override {
    return std::vector<uint64_t>();
  }
  void SetTouchEventLoggingEnabled(bool enabled) override {
    NOTIMPLEMENTED_LOG_ONCE();
  }
  void SetTouchpadSensitivity(int value) override {}
  void SetTouchpadScrollSensitivity(int value) override {}
  void SetTapToClick(bool enabled) override {}
  void SetThreeFingerClick(bool enabled) override {}
  void SetTapDragging(bool enabled) override {}
  void SetNaturalScroll(bool enabled) override {}
  void SetMouseSensitivity(int value) override {}
  void SetMouseScrollSensitivity(int value) override {}
  void SetPrimaryButtonRight(bool right) override {}
  void SetMouseReverseScroll(bool enabled) override {}
  void SetMouseAcceleration(bool enabled) override {}
  void SuspendMouseAcceleration() override {}
  void EndMouseAccelerationSuspension() override {}
  void SetMouseScrollAcceleration(bool enabled) override {}
  void SetPointingStickSensitivity(int value) override {}
  void SetPointingStickPrimaryButtonRight(bool right) override {}
  void SetPointingStickAcceleration(bool enabled) override {}
  void SetGamepadKeyBitsMapping(
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) override {}
  std::vector<uint64_t> GetGamepadKeyBits(int id) override {
    return std::vector<uint64_t>();
  }
  void SetTouchpadAcceleration(bool enabled) override {}
  void SetTouchpadScrollAcceleration(bool enabled) override {}
  void SetTouchpadHapticFeedback(bool enabled) override {}
  void SetTouchpadHapticClickSensitivity(int value) override {}
  void SetTapToClickPaused(bool state) override {}
  void GetTouchDeviceStatus(GetTouchDeviceStatusReply reply) override {
    std::move(reply).Run(std::string());
  }
  void GetTouchEventLog(const base::FilePath& out_dir,
                        GetTouchEventLogReply reply) override {
    std::move(reply).Run(std::vector<base::FilePath>());
  }
  void SetInternalTouchpadEnabled(bool enabled) override {}
  bool IsInternalTouchpadEnabled() const override { return false; }
  void SetTouchscreensEnabled(bool enabled) override {}
  void GetStylusSwitchState(GetStylusSwitchStateReply reply) override {
    std::move(reply).Run(ui::StylusState::REMOVED);
  }
  void SetInternalKeyboardFilter(bool enable_filter,
                                 std::vector<DomCode> allowed_keys) override {}
  void GetGesturePropertiesService(
      mojo::PendingReceiver<ui::ozone::mojom::GesturePropertiesService>
          receiver) override {}
  void PlayVibrationEffect(int id,
                           uint8_t amplitude,
                           uint16_t duration_millis) override {}
  void StopVibration(int id) override {}
  void PlayHapticTouchpadEffect(
      HapticTouchpadEffect effect_type,
      HapticTouchpadEffectStrength strength) override {}
  void SetHapticTouchpadEffectForNextButtonRelease(
      HapticTouchpadEffect effect_type,
      HapticTouchpadEffectStrength strength) override {}
};

}  // namespace

std::unique_ptr<InputController> CreateStubInputController() {
  return std::make_unique<StubInputController>();
}

}  // namespace ui
