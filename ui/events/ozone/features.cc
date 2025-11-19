// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/features.h"

namespace ui {

BASE_FEATURE(kBlockTelephonyDevicePhoneMute, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFastTouchpadClick, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableHeatmapPalmDetection, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableHeuristicPalmDetectionFilter,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableKeyboardUsedPalmSuppression,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableNeuralPalmDetectionFilter,
             base::FEATURE_DISABLED_BY_DEFAULT);

// Used for marking the whole screen as a palm when any palm is detected.
BASE_FEATURE(kEnablePalmSuppression, base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether libinput is used to handle touchpad.
BASE_FEATURE(kLibinputHandleTouchpad, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFakeKeyboardHeuristic, base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFakeMouseHeuristic, base::FEATURE_ENABLED_BY_DEFAULT);

extern const base::FeatureParam<std::string> kNeuralPalmRadiusPolynomial{
    &kEnableNeuralPalmDetectionFilter, "neural_palm_radius_polynomial", ""};

extern const base::FeatureParam<std::string> kNeuralPalmModelVersion{
    &kEnableNeuralPalmDetectionFilter, "neural_palm_model_version", ""};

const base::FeatureParam<double> kHeuristicCancelThresholdSeconds{
    &kEnableHeuristicPalmDetectionFilter,
    "heuristic_palm_cancel_threshold_seconds", 0.4};

const base::FeatureParam<double> kHeuristicHoldThresholdSeconds{
    &kEnableHeuristicPalmDetectionFilter,
    "heuristic_palm_hold_threshold_seconds", 1.0};

const base::FeatureParam<int> kHeuristicStrokeCount{
    &kEnableHeuristicPalmDetectionFilter, "heuristic_palm_stroke_count", 0};

BASE_FEATURE(kEnableInputEventLogging, base::FEATURE_DISABLED_BY_DEFAULT);

constexpr char kOzoneNNPalmSwitchName[] = "ozone-nnpalm-properties";

constexpr char kOzoneNNPalmTouchCompatibleProperty[] = "touch-compatible";
constexpr char kOzoneNNPalmModelVersionProperty[] = "model";
constexpr char kOzoneNNPalmRadiusPolynomialProperty[] = "radius-polynomial";

}  // namespace ui
