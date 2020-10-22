// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_FEATURES_H_
#define UI_EVENTS_OZONE_FEATURES_H_

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace ui {
COMPONENT_EXPORT(EVENTS_OZONE)
extern const base::Feature kEnableHeuristicPalmDetectionFilter;

COMPONENT_EXPORT(EVENTS_OZONE)
extern const base::Feature kEnableNeuralPalmDetectionFilter;

COMPONENT_EXPORT(EVENTS_OZONE)
extern const base::Feature kEnableNeuralStylusReportFilter;

COMPONENT_EXPORT(EVENTS_OZONE)
extern const base::Feature kEnableOrdinalMotion;

COMPONENT_EXPORT(EVENTS_OZONE)
extern const base::Feature kEnablePalmOnMaxTouchMajor;

COMPONENT_EXPORT(EVENTS_OZONE)
extern const base::Feature kEnablePalmOnToolTypePalm;

COMPONENT_EXPORT(EVENTS_OZONE)
extern const base::FeatureParam<std::string> kNeuralPalmRadiusPolynomial;

COMPONENT_EXPORT(EVENTS_OZONE)
extern const base::Feature kEnablePalmSuppression;

COMPONENT_EXPORT(EVENTS_OZONE)
extern const base::FeatureParam<double> kHeuristicCancelThresholdSeconds;

COMPONENT_EXPORT(EVENTS_OZONE)
extern const base::FeatureParam<double> kHeuristicHoldThresholdSeconds;

COMPONENT_EXPORT(EVENTS_OZONE)
extern const base::FeatureParam<int> kHeuristicStrokeCount;

COMPONENT_EXPORT(EVENTS_OZONE)
extern const char kOzoneNNPalmSwitchName[];

COMPONENT_EXPORT(EVENTS_OZONE)
extern const char kOzoneNNPalmTouchCompatibleProperty[];

COMPONENT_EXPORT(EVENTS_OZONE)
extern const char kOzoneNNPalmRadiusPolynomialProperty[];

}  // namespace ui

#endif  // UI_EVENTS_OZONE_FEATURES_H_
