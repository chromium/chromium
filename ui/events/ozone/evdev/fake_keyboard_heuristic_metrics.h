// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_EVDEV_FAKE_KEYBOARD_HEURISTIC_METRICS_H_
#define UI_EVENTS_OZONE_EVDEV_FAKE_KEYBOARD_HEURISTIC_METRICS_H_

#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"

namespace ui {

class FakeKeyboardHeuristicMetrics
    : public ash::feature_usage::FeatureUsageMetrics::Delegate {
 public:
  explicit FakeKeyboardHeuristicMetrics();
  ~FakeKeyboardHeuristicMetrics() override;

  bool IsEligible() const override;
  bool IsEnabled() const override;
  void RecordUsage(bool success);

 private:
  ash::feature_usage::FeatureUsageMetrics feature_usage_metrics_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_EVDEV_FAKE_KEYBOARD_HEURISTIC_METRICS_H_
