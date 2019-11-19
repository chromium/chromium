// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/stl_util.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"

namespace chromeos {
namespace input_method {
namespace {

const char *kISOLevel5ShiftLayoutIds[] = {
  "ca(multix)",
  "de(neo)",
};

const char *kAltGrLayoutIds[] = {
  "be",
  "be",
  "be",
  "bg",
  "bg(phonetic)",
  "br",
  "ca",
  "ca(eng)",
  "ca(multix)",
  "ch",
  "ch(fr)",
  "cz",
  "de",
  "de(neo)",
  "dk",
  "ee",
  "es",
  "es(cat)",
  "fi",
  "fr",
  "gb(dvorak)",
  "gb(extd)",
  "gr",
  "hr",
  "il",
  "it",
  "latam",
  "lt",
  "no",
  "pl",
  "pt",
  "ro",
  "se",
  "si",
  "sk",
  "tr",
  "ua",
  "us(altgr-intl)",
  "us(intl)",
};

} // namespace

ImeKeyboard::ImeKeyboard()
    : caps_lock_is_enabled_(false) {
}

ImeKeyboard::~ImeKeyboard() = default;

void ImeKeyboard::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ImeKeyboard::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool ImeKeyboard::SetCurrentKeyboardLayoutByName(
    const std::string& layout_name) {
  for (ImeKeyboard::Observer& observer : observers_)
    observer.OnLayoutChanging(layout_name);
  return true;
}

void ImeKeyboard::SetCapsLockEnabled(bool enable_caps_lock) {
  bool old_state = caps_lock_is_enabled_;
  caps_lock_is_enabled_ = enable_caps_lock;
  if (old_state != enable_caps_lock) {
    for (ImeKeyboard::Observer& observer : observers_)
      observer.OnCapsLockChanged(enable_caps_lock);
  }
}

bool ImeKeyboard::CapsLockIsEnabled() {
  return caps_lock_is_enabled_;
}

bool ImeKeyboard::IsISOLevel5ShiftAvailable() const {
  for (const auto* id : kISOLevel5ShiftLayoutIds) {
    if (last_layout_ == id)
      return true;
  }
  return false;
}

bool ImeKeyboard::IsAltGrAvailable() const {
  for (const auto* id : kAltGrLayoutIds) {
    if (last_layout_ == id)
      return true;
  }
  return false;
}

}  // namespace input_method
}  // namespace chromeos
