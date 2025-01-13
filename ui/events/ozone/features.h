// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_FEATURES_H_
#define UI_EVENTS_OZONE_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace ui {
COMPONENT_EXPORT(EVENTS_OZONE)
BASE_DECLARE_FEATURE(kBlockTelephonyDevicePhoneMute);

COMPONENT_EXPORT(EVENTS_OZONE)
BASE_DECLARE_FEATURE(kEnableFastTouchpadClick);

COMPONENT_EXPORT(EVENTS_OZONE)
BASE_DECLARE_FEATURE(kEnableHeatmapPalmDetection);

COMPONENT_EXPORT(EVENTS_OZONE)
BASE_DECLARE_FEATURE(kEnableHeuristicPalmDetectionFilter);

COMPONENT_EXPORT(EVENTS_OZONE)
BASE_DECLARE_FEATURE(kEnableNeuralPalmDetectionFilter);

COMPONENT_EXPORT(EVENTS_OZONE)
BASE_DECLARE_FEATURE(kEnableNeuralPalmAdaptiveHold);

COMPONENT_EXPORT(EVENTS_OZONE)
BASE_DECLARE_FEATURE(kEnableEdgeDetection);

COMPONENT_EXPORT(EVENTS_OZONE) BASE_DECLARE_FEATURE(kEnableOrdinalMotion);

COMPONENT_EXPORT(EVENTS_OZONE) BASE_DECLARE_FEATURE(kEnablePalmOnMaxTouchMajor);

COMPONENT_EXPORT(EVENTS_OZONE) BASE_DECLARE_FEATURE(kEnablePalmOnToolTypePalm);

COMPONENT_EXPORT(EVENTS_OZONE)
extern const base::FeatureParam<std::string> kNeuralPalmModelVersion;

COMPONENT_EXPORT(EVENTS_OZONE)
extern const base::FeatureParam<std::string> kNeuralPalmRadiusPolynomial;

COMPONENT_EXPORT(EVENTS_OZONE) BASE_DECLARE_FEATURE(kEnablePalmSuppression);

COMPONENT_EXPORT(EVENTS_OZONE)
extern const base::FeatureParam<double> kHeuristicCancelThresholdSeconds;

COMPONENT_EXPORT(EVENTS_OZONE)
extern const base::FeatureParam<double> kHeuristicHoldThresholdSeconds;

COMPONENT_EXPORT(EVENTS_OZONE)
extern const base::FeatureParam<int> kHeuristicStrokeCount;

COMPONENT_EXPORT(EVENTS_OZONE) BASE_DECLARE_FEATURE(kEnableInputEventLogging);

COMPONENT_EXPORT(EVENTS_OZONE) BASE_DECLARE_FEATURE(kLibinputHandleTouchpad);

COMPONENT_EXPORT(EVENTS_OZONE)
BASE_DECLARE_FEATURE(kEnableFakeKeyboardHeuristic);

COMPONENT_EXPORT(EVENTS_OZONE)
BASE_DECLARE_FEATURE(kEnableFakeMouseHeuristic);

COMPONENT_EXPORT(EVENTS_OZONE)
extern const char kOzoneNNPalmSwitchName[];

COMPONENT_EXPORT(EVENTS_OZONE)
extern const char kOzoneNNPalmTouchCompatibleProperty[];

COMPONENT_EXPORT(EVENTS_OZONE)
extern const char kOzoneNNPalmModelVersionProperty[];

COMPONENT_EXPORT(EVENTS_OZONE)
extern const char kOzoneNNPalmRadiusPolynomialProperty[];
}  // namespace ui

#endif  // UI_EVENTS_OZONE_FEATURES_H_
