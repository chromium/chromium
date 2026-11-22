// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/activation_key_tracker_linux.h"

#include <utility>

#include "base/notreached.h"
#include "ui/aura/env.h"
#include "ui/events/event.h"

namespace ui {

// static
ActivationKeyTracker* ActivationKeyTracker::GetInstance() {
  static base::NoDestructor<ActivationKeyTracker> instance;
  return instance.get();
}

ActivationKeyTracker::ActivationKeyTracker() {
  EnsureObserving();
}

ActivationKeyTracker::~ActivationKeyTracker() {
  NOTREACHED();
}

void ActivationKeyTracker::EnsureObserving() {
  if (!observing_ && aura::Env::HasInstance()) {
    aura::Env::GetInstance()->AddPreTargetHandler(this);
    aura::Env::GetInstance()->AddObserver(this);
    observing_ = true;
  }
}

bool ActivationKeyTracker::IsActivationKeyDown() const {
  return is_down_for_testing_.value_or(!pressed_keys_.empty());
}

void ActivationKeyTracker::WaitForKeyRelease(base::OnceClosure callback) {
  if (!IsActivationKeyDown()) {
    std::move(callback).Run();
    return;
  }
  release_callbacks_.push_back(std::move(callback));
}

void ActivationKeyTracker::SetActivationKeyDownForTesting(bool is_down) {
  is_down_for_testing_ = is_down;
  if (!is_down) {
    NotifyCallbacks();
  }
}

void ActivationKeyTracker::ResetForTesting() {
  is_down_for_testing_.reset();
  pressed_keys_.clear();
  release_callbacks_.clear();
}

void ActivationKeyTracker::OnKeyEvent(ui::KeyEvent* event) {
  if (is_down_for_testing_.has_value()) {
    return;
  }
  if (!IsActivationKey(event->key_code())) {
    return;
  }
  if (event->type() == ui::EventType::kKeyPressed) {
    pressed_keys_.insert(event->key_code());
  } else if (event->type() == ui::EventType::kKeyReleased) {
    pressed_keys_.erase(event->key_code());
    if (pressed_keys_.empty()) {
      NotifyCallbacks();
    }
  }
}

void ActivationKeyTracker::OnCancelMode(ui::CancelModeEvent* event) {
  if (is_down_for_testing_.has_value()) {
    return;
  }
  pressed_keys_.clear();
  NotifyCallbacks();
}

void ActivationKeyTracker::OnWillDestroyEnv() {
  if (observing_ && aura::Env::HasInstance()) {
    aura::Env::GetInstance()->RemovePreTargetHandler(this);
    aura::Env::GetInstance()->RemoveObserver(this);
  }
  observing_ = false;
  pressed_keys_.clear();
  NotifyCallbacks();
}

// static
bool ActivationKeyTracker::IsActivationKey(ui::KeyboardCode key_code) {
  return key_code == ui::VKEY_RETURN || key_code == ui::VKEY_SPACE;
}

void ActivationKeyTracker::NotifyCallbacks() {
  auto callbacks = std::move(release_callbacks_);
  release_callbacks_.clear();
  for (auto& cb : callbacks) {
    if (cb) {
      std::move(cb).Run();
    }
  }
}

}  // namespace ui
