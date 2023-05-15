// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/gamepad_event_converter_evdev.h"

#include <errno.h>
#include <linux/input.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/ioctl.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/trace_event/trace_event.h"
#include "ui/events/event_utils.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/gamepad/gamepad_event.h"
#include "ui/events/ozone/gamepad/gamepad_provider_ozone.h"

namespace {
constexpr int kInvalidEffectId = -1;
}

namespace ui {

GamepadEventConverterEvdev::GamepadEventConverterEvdev(
    base::ScopedFD fd,
    base::FilePath path,
    int id,
    const EventDeviceInfo& devinfo,
    DeviceEventDispatcherEvdev* dispatcher)
    : EventConverterEvdev(fd.get(),
                          path,
                          id,
                          devinfo.device_type(),
                          devinfo.name(),
                          devinfo.phys(),
                          devinfo.vendor_id(),
                          devinfo.product_id(),
                          devinfo.version()),
      will_send_frame_(false),
      input_device_fd_(std::move(fd)),
      dispatcher_(dispatcher),
      effect_id_(-1) {
  input_absinfo abs_info;
  for (int code = 0; code < ABS_CNT; ++code) {
    abs_info = devinfo.GetAbsInfoByCode(code);
    if (devinfo.HasAbsEvent(code)) {
      ui::GamepadDevice::Axis axis;
      axis.code = code;
      axis.min_value = abs_info.minimum;
      axis.max_value = abs_info.maximum;
      axis.flat = abs_info.flat;
      axis.fuzz = abs_info.fuzz;
      axis.resolution = abs_info.resolution;
      axes_.push_back(axis);
    }
  }
  supports_rumble_ = devinfo.SupportsRumble();
  // Converts unsigned long to uint64_t.
  const auto key_bits = devinfo.GetKeyBits();
  key_bits_.resize(EVDEV_BITS_TO_INT64(KEY_CNT));
  for (int i = 0; i < KEY_CNT; i++) {
    if (EvdevBitIsSet(key_bits.data(), i)) {
      EvdevSetUint64Bit(key_bits_.data(), i);
    }
  }
}

GamepadEventConverterEvdev::~GamepadEventConverterEvdev() {
  DCHECK(!IsEnabled());
}

bool GamepadEventConverterEvdev::HasGamepad() const {
  return true;
}

void GamepadEventConverterEvdev::PlayVibrationEffect(uint8_t amplitude,
                                                     uint16_t duration_millis) {
  constexpr uint16_t kRumbleMagnitudeMax = 0xFFFF;
  constexpr uint8_t kAmplitudeMax = 0xFF;

  // only rumble type force feedback is supported at the moment
  if (!supports_rumble_) {
    LOG(ERROR) << "Device doesn't support rumble, but SetVibration is called.";
    return;
  }

  float amplitude_ratio = static_cast<float>(amplitude) / kAmplitudeMax;
  uint16_t magnitude_scaled =
      static_cast<uint16_t>(amplitude_ratio * kRumbleMagnitudeMax);

  effect_id_ = StoreRumbleEffect(input_device_fd_, effect_id_, duration_millis,
                                 0, magnitude_scaled, magnitude_scaled);

  if (effect_id_ == kInvalidEffectId) {
    LOG(ERROR) << "SetVibration is called with an invalid effect ID.";
    return;
  }
  StartOrStopEffect(input_device_fd_, effect_id_, true);
}

void GamepadEventConverterEvdev::StopVibration() {
  if (!supports_rumble_) {
    LOG(ERROR)
        << "Device doesn't support rumble, but SetZeroVibration is called.";
    return;
  }
  if (effect_id_ == kInvalidEffectId) {
    LOG(ERROR) << "SetZeroVibration is called with an invalid effect ID.";
    return;
  }
  StartOrStopEffect(input_device_fd_, effect_id_, false);
}

int GamepadEventConverterEvdev::StoreRumbleEffect(const base::ScopedFD& fd,
                                                  int effect_id,
                                                  uint16_t duration,
                                                  uint16_t start_delay,
                                                  uint16_t strong_magnitude,
                                                  uint16_t weak_magnitude) {
  struct ff_effect effect;
  memset(&effect, 0, sizeof(effect));
  effect.type = FF_RUMBLE;
  effect.id = effect_id;
  effect.replay.length = duration;
  effect.replay.delay = start_delay;
  effect.u.rumble.strong_magnitude = strong_magnitude;
  effect.u.rumble.weak_magnitude = weak_magnitude;

  return UploadFfEffect(fd, &effect);
}

void GamepadEventConverterEvdev::StartOrStopEffect(const base::ScopedFD& fd,
                                                   int effect_id,
                                                   bool do_start) {
  struct input_event start_stop;
  memset(&start_stop, 0, sizeof(start_stop));
  start_stop.type = EV_FF;
  start_stop.code = effect_id;
  start_stop.value = do_start ? 1 : 0;
  ssize_t nbytes = WriteEvent(fd, start_stop);

  if (nbytes != sizeof(start_stop)) {
    PLOG(ERROR) << "Error writing the event in StartOrStopEffect";
  }
}

void GamepadEventConverterEvdev::DestroyEffect(const base::ScopedFD& fd,
                                               int effect_id) {
  if (HANDLE_EINTR(ioctl(fd.get(), EVIOCRMFF, effect_id)) < 0) {
    PLOG(ERROR) << "Error destroying rumble effect.";
  }
  effect_id_ = kInvalidEffectId;
}

ssize_t GamepadEventConverterEvdev::WriteEvent(
    const base::ScopedFD& fd,
    const struct input_event& event) {
  return HANDLE_EINTR(
      write(fd.get(), static_cast<const void*>(&event), sizeof(event)));
}

int GamepadEventConverterEvdev::UploadFfEffect(const base::ScopedFD& fd,
                                               struct ff_effect* effect) {
  if (HANDLE_EINTR(ioctl(fd.get(), EVIOCSFF, effect)) < 0) {
    PLOG(ERROR) << "Error storing rumble effect.";
    return kInvalidEffectId;
  }

  return effect->id;
}

void GamepadEventConverterEvdev::OnFileCanReadWithoutBlocking(int fd) {
  TRACE_EVENT1("evdev",
               "GamepadEventConverterEvdev::OnFileCanReadWithoutBlocking", "fd",
               fd);
  while (true) {
    input_event input;
    ssize_t read_size = read(fd, &input, sizeof(input));
    if (read_size != sizeof(input)) {
      if (errno == EINTR || errno == EAGAIN)
        return;
      if (errno != ENODEV)
        PLOG(ERROR) << "error reading device " << path_.value();
      Stop();
      return;
    }

    if (!IsEnabled())
      return;

    ProcessEvent(input);
  }
}
void GamepadEventConverterEvdev::OnDisabled() {
  ResetGamepad();
}

std::vector<ui::GamepadDevice::Axis>
GamepadEventConverterEvdev::GetGamepadAxes() const {
  return axes_;
}

bool GamepadEventConverterEvdev::GetGamepadRumbleCapability() const {
  return supports_rumble_;
}

std::vector<uint64_t> GamepadEventConverterEvdev::GetGamepadKeyBits() const {
  return key_bits_;
}

void GamepadEventConverterEvdev::ProcessEvent(const input_event& evdev_ev) {
  base::TimeTicks timestamp = TimeTicksFromInputEvent(evdev_ev);
  // We may have missed Gamepad releases. Reset everything.
  // If the event is sync, we send a frame.
  if (evdev_ev.type == EV_SYN) {
    if (evdev_ev.code == SYN_DROPPED) {
      LOG(WARNING) << "kernel dropped input events";
      ResyncGamepad();
    } else if (evdev_ev.code == SYN_REPORT) {
      OnSync(timestamp);
    }
  } else if (evdev_ev.type == EV_KEY) {
    ProcessEvdevKey(evdev_ev.code, evdev_ev.value, timestamp);
  } else if (evdev_ev.type == EV_ABS) {
    ProcessEvdevAbs(evdev_ev.code, evdev_ev.value, timestamp);
  }
}

void GamepadEventConverterEvdev::ProcessEvdevKey(
    uint16_t code,
    int value,
    const base::TimeTicks& timestamp) {
  if (value)
    pressed_buttons_.insert(code);
  else
    pressed_buttons_.erase(code);
  GamepadEvent event(input_device_.id, GamepadEventType::BUTTON, code, value,
                     timestamp);
  dispatcher_->DispatchGamepadEvent(event);
  will_send_frame_ = true;
}

void GamepadEventConverterEvdev::ProcessEvdevAbs(
    uint16_t code,
    int value,
    const base::TimeTicks& timestamp) {
  GamepadEvent event(input_device_.id, GamepadEventType::AXIS, code, value,
                     timestamp);
  dispatcher_->DispatchGamepadEvent(event);
  will_send_frame_ = true;
}

void GamepadEventConverterEvdev::ResetGamepad() {
  base::TimeTicks timestamp = ui::EventTimeForNow();
  if (effect_id_ != kInvalidEffectId) {
    DestroyEffect(input_device_fd_, effect_id_);
  }
  // Reset all the buttons.
  for (unsigned int code : pressed_buttons_)
    ProcessEvdevKey(code, 0, timestamp);
  // Reset all the axes.
  for (int code = 0; code < ABS_CNT; ++code)
    ProcessEvdevAbs(code, 0, timestamp);
  OnSync(timestamp);
}

void GamepadEventConverterEvdev::ResyncGamepad() {
  base::TimeTicks timestamp = ui::EventTimeForNow();
  // Reset all the buttons.
  for (unsigned int code : pressed_buttons_)
    ProcessEvdevKey(code, 0, timestamp);
  // Read the state of all axis.
  EventDeviceInfo info;
  if (!info.Initialize(fd_, path_)) {
    LOG(ERROR) << "Failed to synchronize state for gamepad device: "
               << path_.value();
    Stop();
    return;
  }
  for (int code = 0; code < ABS_CNT; ++code) {
    if (info.HasAbsEvent(code)) {
      ProcessEvdevAbs(code, info.GetAbsValue(code), timestamp);
    }
  }
  OnSync(timestamp);
}

void GamepadEventConverterEvdev::OnSync(const base::TimeTicks& timestamp) {
  if (will_send_frame_) {
    GamepadEvent event(input_device_.id, GamepadEventType::FRAME, 0, 0,
                       timestamp);
    dispatcher_->DispatchGamepadEvent(event);
    will_send_frame_ = false;
  }
}

std::ostream& GamepadEventConverterEvdev::DescribeForLog(
    std::ostream& os) const {
  os << "class=ui::GamepadEventConverterEvdev id=" << input_device_.id
     << std::endl
     << " supports_rumble=" << supports_rumble_ << std::endl
     << "base ";
  return EventConverterEvdev::DescribeForLog(os);
}

}  //  namespace ui
