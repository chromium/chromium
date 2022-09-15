// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/libgestures_glue/haptic_touchpad_handler.h"

#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "ui/events/ozone/evdev/input_device_settings_evdev.h"

// TODO(b/205702807): Remove these definitions when the new version of input.h
// is rolled in.
#ifndef EVIOCFFTAKECONTROL
#define EVIOCFFTAKECONTROL _IOW('E', 0x82, int)
#define EVIOCFFRELEASECONTROL _IOW('E', 0x83, int)

#define FF_HID 0x4f

struct ff_hid_effect {
  __u16 hid_usage;
  __u16 vendor_id;
  __u8 vendor_waveform_page;
  __u16 intensity;
  __u16 repeat_count;
  __u16 retrigger_period;
};

typedef struct local_ff_effect {
  __u16 type;
  __s16 id;
  __u16 direction;
  struct ff_trigger trigger;
  struct ff_replay replay;

  union {
    struct ff_constant_effect constant;
    struct ff_ramp_effect ramp;
    struct ff_periodic_effect periodic;
    struct ff_condition_effect condition[2];
    struct ff_rumble_effect rumble;
    struct ff_hid_effect hid;
  } u;
} local_ff_effect;
#else
using local_ff_effect = ff_effect;
#endif

namespace ui {

namespace {
constexpr int kInvalidEffectId = -1;
constexpr int kReleaseEffectId = 0;
constexpr int kPressEffectId = 1;

constexpr uint16_t kGoogleVendorId = 0x18d1;
constexpr uint8_t kHIDWaveformUsagePage = 0x01;

constexpr struct {
  HapticTouchpadEffect effect;
  uint16_t usage;
  struct {
    HapticTouchpadEffectStrength strength;
    uint16_t percent;
  } strength_to_percent[3];
} kEffectToHIDUsage[] = {
    {HapticTouchpadEffect::kSnap,
     0x2003,
     {{HapticTouchpadEffectStrength::kLow, 20},
      {HapticTouchpadEffectStrength::kMedium, 50},
      {HapticTouchpadEffectStrength::kHigh, 80}}},
    {HapticTouchpadEffect::kSnapReverse,
     0x2004,
     {{HapticTouchpadEffectStrength::kLow, 20},
      {HapticTouchpadEffectStrength::kMedium, 50},
      {HapticTouchpadEffectStrength::kHigh, 80}}},
    {HapticTouchpadEffect::kKnock,
     0x2005,
     {{HapticTouchpadEffectStrength::kLow, 20},
      {HapticTouchpadEffectStrength::kMedium, 50},
      {HapticTouchpadEffectStrength::kHigh, 80}}},
    {HapticTouchpadEffect::kTick,
     0x2006,
     {{HapticTouchpadEffectStrength::kLow, 20},
      {HapticTouchpadEffectStrength::kMedium, 50},
      {HapticTouchpadEffectStrength::kHigh, 70}}},
    {HapticTouchpadEffect::kToggleOn,
     0x2007,
     {{HapticTouchpadEffectStrength::kLow, 15},
      {HapticTouchpadEffectStrength::kMedium, 30},
      {HapticTouchpadEffectStrength::kHigh, 50}}},
    {HapticTouchpadEffect::kToggleOff,
     0x2008,
     {{HapticTouchpadEffectStrength::kLow, 20},
      {HapticTouchpadEffectStrength::kMedium, 50},
      {HapticTouchpadEffectStrength::kHigh, 80}}},
    {HapticTouchpadEffect::kPress,
     0x1006,
     {{HapticTouchpadEffectStrength::kLow, 75},
      {HapticTouchpadEffectStrength::kMedium, 90},
      {HapticTouchpadEffectStrength::kHigh, 90}}},
    {HapticTouchpadEffect::kRelease,
     0x1007,
     {{HapticTouchpadEffectStrength::kLow, 55},
      {HapticTouchpadEffectStrength::kMedium, 65},
      {HapticTouchpadEffectStrength::kHigh, 65}}},
    {HapticTouchpadEffect::kDeepPress,
     0x2001,
     {{HapticTouchpadEffectStrength::kLow, 35},
      {HapticTouchpadEffectStrength::kMedium, 60},
      {HapticTouchpadEffectStrength::kHigh, 80}}},
    {HapticTouchpadEffect::kDeepRelease,
     0x2002,
     {{HapticTouchpadEffectStrength::kLow, 20},
      {HapticTouchpadEffectStrength::kMedium, 40},
      {HapticTouchpadEffectStrength::kHigh, 70}}},
};

}  // namespace

// static
std::unique_ptr<HapticTouchpadHandler> HapticTouchpadHandler::Create(
    int fd,
    const EventDeviceInfo& devinfo) {
  if (!devinfo.HasHapticTouchpad())
    return nullptr;
  auto handler = std::make_unique<HapticTouchpadHandler>(fd);
  handler->Initialize();
  if (!handler->IsValid()) {
    return nullptr;
  }
  return handler;
}

HapticTouchpadHandler::HapticTouchpadHandler(int fd)
    : fd_(fd),
      next_click_release_effect_(HapticTouchpadEffect::kRelease),
      click_strength_(HapticTouchpadEffectStrength::kMedium),
      button_pressed_(false) {}

void HapticTouchpadHandler::Initialize() {
  haptics_enabled_ = SetupFf();
  if (!haptics_enabled_) {
    ReleaseControlOfClickEffects();
    DestroyAllFfEffects();
  }
}

HapticTouchpadHandler::~HapticTouchpadHandler() = default;

bool HapticTouchpadHandler::IsValid() {
  return haptics_enabled_;
}

void HapticTouchpadHandler::SetEffectForNextButtonRelease(
    HapticTouchpadEffect effect,
    HapticTouchpadEffectStrength strength) {
  if (!button_pressed_)
    return;
  next_click_release_effect_ = effect;
  next_click_release_strength_ = strength;
}

void HapticTouchpadHandler::SetClickStrength(
    HapticTouchpadEffectStrength strength) {
  click_strength_ = strength;
}

void HapticTouchpadHandler::OnGestureClick(bool press) {
  button_pressed_ = press;
  if (press) {
    PlayEffect(HapticTouchpadEffect::kPress, click_strength_);
  } else {
    HapticTouchpadEffectStrength strength;
    if (next_click_release_effect_ == HapticTouchpadEffect::kRelease) {
      strength = click_strength_;
    } else {
      strength = next_click_release_strength_;
    }
    PlayEffect(next_click_release_effect_, strength);
    next_click_release_effect_ = HapticTouchpadEffect::kRelease;
  }
}

void HapticTouchpadHandler::PlayEffect(HapticTouchpadEffect effect,
                                       HapticTouchpadEffectStrength strength) {
  PlayFfEffect(ff_effect_id_[effect][strength]);
}

bool HapticTouchpadHandler::SetupFf() {
  if (!TakeControlOfClickEffects())
    return false;

  for (const auto& effect_data : kEffectToHIDUsage) {
    for (const auto& strength_pair : effect_data.strength_to_percent) {
      int effect_id = UploadFfEffect(effect_data.usage, strength_pair.percent);
      if (effect_id == kInvalidEffectId)
        return false;
      ff_effect_id_[effect_data.effect][strength_pair.strength] = effect_id;
    }
  }

  return true;
}

void HapticTouchpadHandler::DestroyAllFfEffects() {
  for (const auto& effect_data : kEffectToHIDUsage) {
    for (const auto& strength_pair : effect_data.strength_to_percent) {
      int effect_id = ff_effect_id_[effect_data.effect][strength_pair.strength];
      DestroyFfEffect(effect_id);
    }
  }
}

void HapticTouchpadHandler::DestroyFfEffect(int effect_id) {
  if (effect_id == kInvalidEffectId)
    return;

  if (HANDLE_EINTR(ioctl(fd_, EVIOCRMFF, effect_id)) < 0) {
    PLOG(ERROR) << "Error destroying force feedback effect.";
  }
}

void HapticTouchpadHandler::PlayFfEffect(int effect_id) {
  struct input_event event;
  memset(&event, 0, sizeof(event));
  event.type = EV_FF;
  event.code = effect_id;
  event.value = 1;

  ssize_t nbytes =
      HANDLE_EINTR(write(fd_, static_cast<const void*>(&event), sizeof(event)));
  if (nbytes != sizeof(event)) {
    PLOG(ERROR) << "Error playing the force feedback effect.";
  }
}

int HapticTouchpadHandler::UploadFfEffect(uint16_t hid_usage,
                                          uint8_t intensity) {
  local_ff_effect effect;
  memset(&effect, 0, sizeof(effect));

  effect.id = kInvalidEffectId;
  effect.type = FF_HID;
  effect.u.hid.hid_usage = hid_usage;
  effect.u.hid.vendor_id = kGoogleVendorId;
  effect.u.hid.vendor_waveform_page = kHIDWaveformUsagePage;
  effect.u.hid.intensity = intensity;
  if (HANDLE_EINTR(ioctl(fd_, EVIOCSFF, &effect)) < 0) {
    PLOG(ERROR) << "Error uploading force feedback effect.";
    return kInvalidEffectId;
  }

  return effect.id;
}

bool HapticTouchpadHandler::TakeControlOfClickEffects() {
  if (HANDLE_EINTR(ioctl(fd_, EVIOCFFTAKECONTROL, kPressEffectId)) < 0) {
    PLOG(ERROR) << "Error taking control of force feedback press effect.";
    return false;
  }
  if (HANDLE_EINTR(ioctl(fd_, EVIOCFFTAKECONTROL, kReleaseEffectId)) < 0) {
    PLOG(ERROR) << "Error taking control of force feedback release effect.";
    return false;
  }
  return true;
}

void HapticTouchpadHandler::ReleaseControlOfClickEffects() {
  if (HANDLE_EINTR(ioctl(fd_, EVIOCFFRELEASECONTROL, kPressEffectId)) < 0) {
    PLOG(ERROR) << "Error releasing control of force feedback press effect.";
  }
  if (HANDLE_EINTR(ioctl(fd_, EVIOCFFRELEASECONTROL, kReleaseEffectId)) < 0) {
    PLOG(ERROR) << "Error releasing control of force feedback release effect.";
  }
}

}  // namespace ui
