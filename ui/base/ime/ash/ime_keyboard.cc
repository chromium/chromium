// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ash/ime_keyboard.h"

#include "base/containers/contains.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"

namespace ash {
namespace input_method {

namespace {

constexpr const char* kISOLevel5ShiftLayoutIds[] = {
    "ca(multix)",
    "de(neo)",
};

constexpr const char* kAltGrLayoutIds[] = {
    "be",         "be",           "be",
    "bg",         "bg(phonetic)", "br",
    "ca",         "ca(eng)",      "ca(multix)",
    "ch",         "ch(fr)",       "cz",
    "de",         "de(neo)",      "dk",
    "ee",         "es",           "es(cat)",
    "fi",         "fr",           "fr(oss)",
    "gb(dvorak)", "gb(extd)",     "gr",
    "hr",         "il",           "it",
    "latam",      "lt",           "no",
    "pl",         "pt",           "ro",
    "se",         "si",           "sk",
    "tr",         "ua",           "us(altgr-intl)",
    "us(intl)",
};

} // namespace

ImeKeyboard::ImeKeyboard() = default;
ImeKeyboard::~ImeKeyboard() = default;

void ImeKeyboard::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ImeKeyboard::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool ImeKeyboard::SetCurrentKeyboardLayoutByName(
    const std::string& layout_name) {
  // Only notify on keyboard layout change.
  if (last_layout_ == layout_name) {
    return false;
  }
  for (ImeKeyboard::Observer& observer : observers_)
    observer.OnLayoutChanging(layout_name);
  last_layout_ = layout_name;
  return true;
}

void ImeKeyboard::SetCapsLockEnabled(bool enable_caps_lock) {
  bool old_state = caps_lock_is_enabled_;
  caps_lock_is_enabled_ = enable_caps_lock;
  if (old_state != enable_caps_lock) {
    base::RecordAction(base::UserMetricsAction("CapsLock_Toggled"));
    for (ImeKeyboard::Observer& observer : observers_)
      observer.OnCapsLockChanged(enable_caps_lock);
  }
}

bool ImeKeyboard::IsCapsLockEnabled() {
  return caps_lock_is_enabled_;
}

bool ImeKeyboard::IsISOLevel5ShiftAvailable() const {
  return base::Contains(kISOLevel5ShiftLayoutIds, last_layout_);
}

bool ImeKeyboard::IsAltGrAvailable() const {
  return base::Contains(kAltGrLayoutIds, last_layout_);
}

}  // namespace input_method
}  // namespace ash
