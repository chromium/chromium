// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_INPUT_PROTECTION_OCCLUSION_AWARE_INPUT_PROTECTION_POLICY_H_
#define UI_VIEWS_INPUT_PROTECTION_OCCLUSION_AWARE_INPUT_PROTECTION_POLICY_H_

#include "ui/views/input_protection/input_protection_policy.h"
#include "ui/views/views_export.h"

namespace views {

class InputEventActivationProtector;
class View;

// An implementation of `InputProtectionPolicy` that protects widgets against
// unintended interaction caused by occlusion from always-on-top windows. It
// queries the `OccludedWidgetInputProtector` singleton to determine occlusion
// status.
//
// It blocks input events in the following cases:
//
// 1. Current occlusion: The event target is currently covered by an
//    always-on-top widget.
// 2. Recent occlusion: The event target was recently covered by an
//    always-on-top widget that was hidden or moved (protecting against
//    "pop-away" or "pop-under" attacks). This protection expires after a
//    cooldown (double-click interval).
class VIEWS_EXPORT OcclusionAwareInputProtectionPolicy final
    : public InputProtectionPolicy {
 public:
  OcclusionAwareInputProtectionPolicy() = default;
  ~OcclusionAwareInputProtectionPolicy() override = default;

  // InputProtectionPolicy:
  bool IsPossiblyUnintendedInteraction(
      const ui::Event& event,
      const View* target_view,
      const InputEventActivationProtector& protector) override;
};

}  // namespace views

#endif  // UI_VIEWS_INPUT_PROTECTION_OCCLUSION_AWARE_INPUT_PROTECTION_POLICY_H_
