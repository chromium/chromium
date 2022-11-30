// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/fake_keyboard_heuristic_metrics.h"

#include "ui/events/ozone/features.h"

namespace ui {

FakeKeyboardHeuristicMetrics::FakeKeyboardHeuristicMetrics()
    : feature_usage_metrics_("FakeKeyboardHeuristic", this) {}

FakeKeyboardHeuristicMetrics::~FakeKeyboardHeuristicMetrics() = default;

bool FakeKeyboardHeuristicMetrics::IsEligible() const {
  return true;
}

bool FakeKeyboardHeuristicMetrics::IsEnabled() const {
  return base::FeatureList::IsEnabled(kEnableFakeKeyboardHeuristic);
}

void FakeKeyboardHeuristicMetrics::RecordUsage(bool success) {
  feature_usage_metrics_.RecordUsage(success);
}

}  // namespace ui
