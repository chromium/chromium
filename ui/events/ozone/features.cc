// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/features.h"

namespace ui {

BASE_FEATURE(kBlockTelephonyDevicePhoneMute,
             "BlockTelephonyDevicePhoneMute",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFastTouchpadClick,
             "EnableFastTouchpadClick",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableHeatmapPalmDetection,
             "EnableHeatmapPalmDetection",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableHeuristicPalmDetectionFilter,
             "EnableHeuristicPalmDetectionFilter",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableNeuralPalmDetectionFilter,
             "EnableNeuralPalmDetectionFilter",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableNeuralPalmAdaptiveHold,
             "EnableNeuralPalmAdaptiveHold",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableEdgeDetection,
             "EnableEdgeDetection",
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(b/171249701): Remove this flag when we can support this in all cases.
BASE_FEATURE(kEnableOrdinalMotion,
             "EnableOrdinalMotion",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnablePalmOnMaxTouchMajor,
             "EnablePalmOnMaxTouchMajor",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnablePalmOnToolTypePalm,
             "EnablePalmOnToolTypePalm",
             base::FEATURE_ENABLED_BY_DEFAULT);

/// Used for marking the whole screen as a palm when any palm is detected.
BASE_FEATURE(kEnablePalmSuppression,
             "EnablePalmSuppression",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether libinput is used to handle touchpad.
BASE_FEATURE(kLibinputHandleTouchpad,
             "LibinputHandleTouchpad",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFakeKeyboardHeuristic,
             "EnableFakeKeyboardHeuristic",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kEnableFakeMouseHeuristic,
             "EnableFakeMouseHeuristic",
             base::FEATURE_ENABLED_BY_DEFAULT);

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

BASE_FEATURE(kEnableInputEventLogging,
             "EnableInputEventLogging",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr char kOzoneNNPalmSwitchName[] = "ozone-nnpalm-properties";

constexpr char kOzoneNNPalmTouchCompatibleProperty[] = "touch-compatible";
constexpr char kOzoneNNPalmModelVersionProperty[] = "model";
constexpr char kOzoneNNPalmRadiusPolynomialProperty[] = "radius-polynomial";

}  // namespace ui
