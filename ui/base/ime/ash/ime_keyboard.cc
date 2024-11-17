// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/ash/ime_keyboard.h"

#include "base/containers/fixed_flat_set.h"
#include "base/functional/callback.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"

namespace ash::input_method {

ImeKeyboard::ImeKeyboard() = default;
ImeKeyboard::~ImeKeyboard() = default;

void ImeKeyboard::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void ImeKeyboard::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ImeKeyboard::SetCurrentKeyboardLayoutByName(
    const std::string& layout_name,
    base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(SetCurrentKeyboardLayoutByNameImpl(layout_name));
}

bool ImeKeyboard::SetCurrentKeyboardLayoutByNameImpl(
    const std::string& layout_name) {
  // Only notify on keyboard layout change.
  if (last_layout_ == layout_name) {
    return false;
  }
  observers_.Notify(&ImeKeyboard::Observer::OnLayoutChanging, layout_name);
  last_layout_ = layout_name;
  return true;
}

void ImeKeyboard::SetCapsLockEnabled(bool enable_caps_lock) {
  bool old_state = caps_lock_is_enabled_;
  caps_lock_is_enabled_ = enable_caps_lock;
  if (old_state != enable_caps_lock) {
    base::RecordAction(base::UserMetricsAction("CapsLock_Toggled"));
    observers_.Notify(&ImeKeyboard::Observer::OnCapsLockChanged,
                      enable_caps_lock);
  }
}

bool ImeKeyboard::IsCapsLockEnabled() {
  return caps_lock_is_enabled_;
}

bool ImeKeyboard::IsISOLevel5ShiftAvailable() const {
  constexpr auto kSet = base::MakeFixedFlatSet<std::string_view>({
      "ca(multix)",
      "de(neo)",
  });

  return kSet.contains(last_layout_);
}

bool ImeKeyboard::IsAltGrAvailable() const {
  constexpr auto kSet = base::MakeFixedFlatSet<std::string_view>({
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
      "fr(oss)",
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
  });

  return kSet.contains(last_layout_);
}

}  // namespace ash::input_method
