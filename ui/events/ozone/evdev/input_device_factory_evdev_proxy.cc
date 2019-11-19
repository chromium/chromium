// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/input_device_factory_evdev_proxy.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/threading/thread_task_runner_handle.h"
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
      base::BindOnce(&InputDeviceFactoryEvdev::GetTouchDeviceStatus,
                     input_device_factory_,
                     base::BindOnce(&ForwardGetTouchDeviceStatusReply,
                                    base::ThreadTaskRunnerHandle::Get(),
                                    std::move(reply))));
}

void InputDeviceFactoryEvdevProxy::GetTouchEventLog(
    const base::FilePath& out_dir,
    InputController::GetTouchEventLogReply reply) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InputDeviceFactoryEvdev::GetTouchEventLog,
                     input_device_factory_, out_dir,
                     base::BindOnce(&ForwardGetTouchEventLogReply,
                                    base::ThreadTaskRunnerHandle::Get(),
                                    std::move(reply))));
}

void InputDeviceFactoryEvdevProxy::GetGesturePropertiesService(
    mojo::PendingReceiver<ozone::mojom::GesturePropertiesService> receiver) {
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&InputDeviceFactoryEvdev::GetGesturePropertiesService,
                     input_device_factory_, std::move(receiver)));
}

}  // namespace ui
