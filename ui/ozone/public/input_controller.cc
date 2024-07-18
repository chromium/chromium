// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/public/input_controller.h"

#include <memory>

#include "base/functional/callback.h"
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
  void SetCurrentLayoutByName(
      const std::string& layout_name,
      base::OnceCallback<void(bool)> callback) override {
    std::move(callback).Run(false);
  }
  void SetKeyboardKeyBitsMapping(
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) override {}
  std::vector<uint64_t> GetKeyboardKeyBits(int id) override {
    return std::vector<uint64_t>();
  }
  void SetTouchEventLoggingEnabled(bool enabled) override {
    NOTIMPLEMENTED_LOG_ONCE();
  }
  void SuspendMouseAcceleration() override {}
  void EndMouseAccelerationSuspension() override {}
  void SetThreeFingerClick(bool enabled) override {}
  void SetTouchpadSensitivity(std::optional<int> device_id,
                              int value) override {}
  void SetTouchpadScrollSensitivity(std::optional<int> device_id,
                                    int value) override {}
  void SetTouchpadHapticFeedback(std::optional<int> device_id,
                                 bool enabled) override {}
  void SetTouchpadHapticClickSensitivity(std::optional<int> device_id,
                                         int value) override {}
  void SetTapToClick(std::optional<int> device_id, bool enabled) override {}
  void SetTapDragging(std::optional<int> device_id, bool enabled) override {}
  void SetNaturalScroll(std::optional<int> device_id, bool enabled) override {}
  void SetMouseSensitivity(std::optional<int> device_id, int value) override {}
  void SetMouseScrollSensitivity(std::optional<int> device_id,
                                 int value) override {}
  void SetMouseReverseScroll(std::optional<int> device_id,
                             bool enabled) override {}
  void SetMouseAcceleration(std::optional<int> device_id,
                            bool enabled) override {}
  void SetMouseScrollAcceleration(std::optional<int> device_id,
                                  bool enabled) override {}
  void SetPointingStickSensitivity(std::optional<int> device_id,
                                   int value) override {}
  void SetPointingStickAcceleration(std::optional<int> device_id,
                                    bool enabled) override {}
  void SetTouchpadAcceleration(std::optional<int> device_id,
                               bool enabled) override {}
  void SetTouchpadScrollAcceleration(std::optional<int> device_id,
                                     bool enabled) override {}
  void SetPrimaryButtonRight(std::optional<int> device_id,
                             bool right) override {}
  void SetPointingStickPrimaryButtonRight(std::optional<int> device_id,
                                          bool right) override {}
  void SetGamepadKeyBitsMapping(
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) override {}
  std::vector<uint64_t> GetGamepadKeyBits(int id) override {
    return std::vector<uint64_t>();
  }
  void SetTapToClickPaused(bool state) override {}
  void GetTouchDeviceStatus(GetTouchDeviceStatusReply reply) override {
    std::move(reply).Run(std::string());
  }
  void GetTouchEventLog(const base::FilePath& out_dir,
                        GetTouchEventLogReply reply) override {
    std::move(reply).Run(std::vector<base::FilePath>());
  }
  void DescribeForLog(DescribeForLogReply reply) const override {
    std::move(reply).Run(std::string());
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
  bool AreAnyKeysPressed() override { return false; }
  void BlockModifiersOnDevices(std::vector<int> device_ids) override {}

  bool AreInputDevicesEnabled() const override {
    return num_scoped_input_devices_disablers_ == 0;
  }
  std::unique_ptr<ScopedDisableInputDevices> DisableInputDevices() override {
    return std::make_unique<ScopedDisableInputDevicesImpl>(*this);
  }

  void DisableKeyboardImposterCheck() override {}

 private:
  int num_scoped_input_devices_disablers_ = 0;
};

}  // namespace

std::unique_ptr<InputController> CreateStubInputController() {
  return std::make_unique<StubInputController>();
}

}  // namespace ui
