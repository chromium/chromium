// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/input_device_factory_evdev_proxy.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/single_thread_task_runner.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/events/ozone/evdev/input_device_factory_evdev.h"

namespace ui {

namespace {

void ForwardGetTouchDeviceStatusReply(
    scoped_refptr<base::SingleThreadTaskRunner> reply_runner,
    InputController::GetTouchDeviceStatusReply reply,
    const std::string& status) {
  // Thread hop back to UI for reply.
  reply_runner->PostTask(FROM_HERE, base::BindOnce(std::move(reply), status));
}

void ForwardGetTouchEventLogReply(
    scoped_refptr<base::SingleThreadTaskRunner> reply_runner,
    InputController::GetTouchEventLogReply reply,
    const std::vector<base::FilePath>& log_paths) {
  // Thread hop back to UI for reply.
  reply_runner->PostTask(FROM_HERE,
                         base::BindOnce(std::move(reply), log_paths));
}

void ForwardGetStylusSwitchStateReply(
    scoped_refptr<base::SingleThreadTaskRunner> reply_runner,
    InputController::GetStylusSwitchStateReply reply,
    ui::StylusState state) {
  // Thread hop back to UI for reply.
  reply_runner->PostTask(FROM_HERE, base::BindOnce(std::move(reply), state));
}

void ForwardDescribeForLogReply(
    scoped_refptr<base::SingleThreadTaskRunner> reply_runner,
    InputController::DescribeForLogReply reply,
    const std::string& result) {
  // Thread hop back to UI for reply.
  reply_runner->PostTask(FROM_HERE, base::BindOnce(std::move(reply), result));
}

}  // namespace

InputDeviceFactoryEvdevProxy::InputDeviceFactoryEvdevProxy(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    base::WeakPtr<InputDeviceFactoryEvdev> input_device_factory)
    : task_runner_(task_runner), input_device_factory_(input_device_factory) {
}

InputDeviceFactoryEvdevProxy::~InputDeviceFactoryEvdevProxy() {
}

void InputDeviceFactoryEvdevProxy::AddInputDevice(int id,
                                                  const base::FilePath& path) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&InputDeviceFactoryEvdev::AddInputDevice,
                                input_device_factory_, id, path));
}

void InputDeviceFactoryEvdevProxy::RemoveInputDevice(
    const base::FilePath& path) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&InputDeviceFactoryEvdev::RemoveInputDevice,
                                input_device_factory_, path));
}

void InputDeviceFactoryEvdevProxy::OnStartupScanComplete() {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&InputDeviceFactoryEvdev::OnStartupScanComplete,
                                input_device_factory_));
}

void InputDeviceFactoryEvdevProxy::SetCapsLockLed(bool enabled) {
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&InputDeviceFactoryEvdev::SetCapsLockLed,
                                input_device_factory_, enabled));
}

void InputDeviceFactoryEvdevProxy::GetStylusSwitchState(
    InputController::GetStylusSwitchStateReply reply) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &InputDeviceFactoryEvdev::GetStylusSwitchState, input_device_factory_,
          base::BindOnce(&ForwardGetStylusSwitchStateReply,
                         base::SingleThreadTaskRunner::GetCurrentDefault(),
                         std::move(reply))));
}

void InputDeviceFactoryEvdevProxy::UpdateInputDeviceSettings(
    const InputDeviceSettingsEvdev& settings) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InputDeviceFactoryEvdev::UpdateInputDeviceSettings,
                     input_device_factory_, settings));
}

void InputDeviceFactoryEvdevProxy::GetTouchDeviceStatus(
    InputController::GetTouchDeviceStatusReply reply) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &InputDeviceFactoryEvdev::GetTouchDeviceStatus, input_device_factory_,
          base::BindOnce(&ForwardGetTouchDeviceStatusReply,
                         base::SingleThreadTaskRunner::GetCurrentDefault(),
                         std::move(reply))));
}

void InputDeviceFactoryEvdevProxy::GetTouchEventLog(
    const base::FilePath& out_dir,
    InputController::GetTouchEventLogReply reply) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &InputDeviceFactoryEvdev::GetTouchEventLog, input_device_factory_,
          out_dir,
          base::BindOnce(&ForwardGetTouchEventLogReply,
                         base::SingleThreadTaskRunner::GetCurrentDefault(),
                         std::move(reply))));
}

void InputDeviceFactoryEvdevProxy::DescribeForLog(
    InputController::DescribeForLogReply reply) const {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &InputDeviceFactoryEvdev::DescribeForLog, input_device_factory_,
          base::BindOnce(&ForwardDescribeForLogReply,
                         base::SingleThreadTaskRunner::GetCurrentDefault(),
                         std::move(reply))));
}

void InputDeviceFactoryEvdevProxy::GetGesturePropertiesService(
    mojo::PendingReceiver<ozone::mojom::GesturePropertiesService> receiver) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InputDeviceFactoryEvdev::GetGesturePropertiesService,
                     input_device_factory_, std::move(receiver)));
}

void InputDeviceFactoryEvdevProxy::PlayVibrationEffect(
    int id,
    uint8_t amplitude,
    uint16_t duration_millis) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InputDeviceFactoryEvdev::PlayVibrationEffect,
                     input_device_factory_, id, amplitude, duration_millis));
}

void InputDeviceFactoryEvdevProxy::StopVibration(int id) {
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(&InputDeviceFactoryEvdev::StopVibration,
                                        input_device_factory_, id));
}

void InputDeviceFactoryEvdevProxy::PlayHapticTouchpadEffect(
    ui::HapticTouchpadEffect effect,
    ui::HapticTouchpadEffectStrength strength) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InputDeviceFactoryEvdev::PlayHapticTouchpadEffect,
                     input_device_factory_, effect, strength));
}

void InputDeviceFactoryEvdevProxy::SetHapticTouchpadEffectForNextButtonRelease(
    ui::HapticTouchpadEffect effect,
    ui::HapticTouchpadEffectStrength strength) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &InputDeviceFactoryEvdev::SetHapticTouchpadEffectForNextButtonRelease,
          input_device_factory_, effect, strength));
}

void InputDeviceFactoryEvdevProxy::DisableKeyboardImposterCheck() {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InputDeviceFactoryEvdev::DisableKeyboardImposterCheck,
                     input_device_factory_));
}

}  // namespace ui
