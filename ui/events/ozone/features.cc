// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/features.h"

namespace ui {

const base::Feature kEnableHeuristicPalmDetectionFilter{
    "EnableHeuristicPalmDetectionFilter", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableNeuralPalmDetectionFilter{
    "EnableNeuralPalmDetectionFilter", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableNeuralStylusReportFilter{
    "EnableNeuralStylusReportFilter", base::FEATURE_DISABLED_BY_DEFAULT};

// TODO(b/171249701): Remove this flag when we can support this in all cases.
const base::Feature kEnableOrdinalMotion{"EnableOrdinalMotion",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnablePalmOnMaxTouchMajor{
    "EnablePalmOnMaxTouchMajor", base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kEnablePalmOnToolTypePalm{"EnablePalmOnToolTypePalm",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

const base::Feature kEnablePalmSuppression{"EnablePalmSuppression",
                                           base::FEATURE_ENABLED_BY_DEFAULT};

extern const base::FeatureParam<std::string> kNeuralPalmRadiusPolynomial{
    &kEnableNeuralPalmDetectionFilter, "neural_palm_radius_polynomial", ""};

const base::FeatureParam<double> kHeuristicCancelThresholdSeconds{
    &kEnableHeuristicPalmDetectionFilter,
    "heuristic_palm_cancel_threshold_seconds", 0.4};

const base::FeatureParam<double> kHeuristicHoldThresholdSeconds{
    &kEnableHeuristicPalmDetectionFilter,
    "heuristic_palm_hold_threshold_seconds", 1.0};

const base::FeatureParam<int> kHeuristicStrokeCount{
    &kEnableHeuristicPalmDetectionFilter, "heuristic_palm_stroke_count", 0};

constexpr char kOzoneNNPalmSwitchName[] = "ozone-nnpalm-properties";

constexpr char kOzoneNNPalmTouchCompatibleProperty[] = "touch-compatible";
constexpr char kOzoneNNPalmRadiusPolynomialProperty[] = "radius-polynomial";

}  // namespace ui
