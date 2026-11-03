// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/imposter_checker_evdev_state.h"

#include "base/check_deref.h"
#include "base/functional/callback.h"

namespace ui {

namespace {

static ImposterCheckerEvdevState* g_instance = nullptr;

}  // namespace

ImposterCheckerEvdevState& ImposterCheckerEvdevState::Get() {
  return CHECK_DEREF(g_instance);
}

bool ImposterCheckerEvdevState::HasInstance() {
  return g_instance != nullptr;
}

ImposterCheckerEvdevState::ImposterCheckerEvdevState() {
  CHECK(!g_instance);
  g_instance = this;
}

ImposterCheckerEvdevState::~ImposterCheckerEvdevState() {
  g_instance = nullptr;
}

bool ImposterCheckerEvdevState::IsKeyboardCheckEnabled() {
  return keyboard_check_enabled_;
}

void ImposterCheckerEvdevState::SetKeyboardCheckEnabled(bool enabled) {
  keyboard_check_enabled_ = enabled;
}

}  // namespace ui
