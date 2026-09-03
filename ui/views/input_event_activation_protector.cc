// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/input_event_activation_protector.h"

#include <utility>

#include "base/command_line.h"
#include "ui/events/event.h"
#include "ui/views/input_protection/default_input_protection_policy.h"
#include "ui/views/input_protection/input_protection_policy.h"
#include "ui/views/metrics.h"
#include "ui/views/views_switches.h"

namespace views {

InputEventActivationProtector::InputEventActivationProtector()
    : cooldown_interval_(GetDoubleClickInterval()) {
  stationarity_observation_.Observe(WindowsStationarityMonitor::GetInstance());
  AddPolicy(std::make_unique<DefaultInputProtectionPolicy>());
}

InputEventActivationProtector::InputEventActivationProtector(
    std::unique_ptr<InputProtectionPolicy> policy)
    : cooldown_interval_(GetDoubleClickInterval()) {
  stationarity_observation_.Observe(WindowsStationarityMonitor::GetInstance());
  AddPolicy(std::move(policy));
}

InputEventActivationProtector::~InputEventActivationProtector() = default;

void InputEventActivationProtector::VisibilityChanged(bool is_visible) {
  for (const auto& policy : policies_) {
    if (is_visible) {
      policy->OnProtectionStarted();
    } else {
      policy->OnProtectionStopped();
    }
  }
}

void InputEventActivationProtector::MaybeUpdateViewProtectedTimeStamp(
    bool force) {
  for (const auto& policy : policies_) {
    if (force) {
      policy->OnProtectionStarted();
    } else {
      policy->OnProtectionReset();
    }
  }
}

bool InputEventActivationProtector::IsPossiblyUnintendedInteraction(
    const ui::Event& event,
    bool allow_key_events,
    const View* target_view) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableInputEventActivationProtectionForTesting))
      [[unlikely]] {
    return false;
  }

  // Allow non-input and allowed key events early.
  if (!event.IsMouseEvent() && !event.IsTouchEvent() &&
      !event.IsGestureEvent()) {
    if (allow_key_events || !event.IsKeyEvent()) {
      return false;
    }
  }

  // Forward to policies to run their actual blocking checks.
  for (const auto& policy : policies_) {
    if (policy->IsPossiblyUnintendedInteraction(event, target_view, *this)) {
      return true;
    }
  }

  return false;
}

void InputEventActivationProtector::AddPolicy(
    std::unique_ptr<InputProtectionPolicy> policy) {
  policies_.push_back(std::move(policy));
}

void InputEventActivationProtector::OnWindowStationaryStateChanged() {
  for (const auto& policy : policies_) {
    policy->OnProtectionReset();
  }
}

void InputEventActivationProtector::ResetForTesting() {
  for (const auto& policy : policies_) {
    policy->OnProtectionStopped();
  }
}

}  // namespace views
