// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter_factory.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/touch_filter/heuristic_stylus_palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/neural_stylus_palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/neural_stylus_palm_detection_filter_model.h"
#include "ui/events/ozone/evdev/touch_filter/open_palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/palm_detection_filter.h"
#include "ui/events/ozone/evdev/touch_filter/palm_model/onedevice_train_palm_detection_filter_model.h"

namespace ui {

const base::Feature kEnableHeuristicPalmDetectionFilter{
    "EnableHeuristicPalmDetectionFilter", base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableNeuralPalmDetectionFilter{
    "EnableNeuralPalmDetectionFilter", base::FEATURE_DISABLED_BY_DEFAULT};

EVENTS_OZONE_EVDEV_EXPORT
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

namespace internal {

std::vector<float> ParseRadiusPolynomial(const std::string& radius_string) {
  std::vector<std::string> split_radii = base::SplitString(
      radius_string, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::vector<float> return_value;
  for (const std::string& unparsed : split_radii) {
    double item;
    if (!base::StringToDouble(unparsed, &item) && !std::isnan(item)) {
      LOG(DFATAL) << "Unable to parse " << unparsed
                  << " to a floating point; Returning empty.";
      return {};
    }
    return_value.push_back(item);
  }
  return return_value;
}

}  // namespace internal

std::unique_ptr<PalmDetectionFilter> CreatePalmDetectionFilter(
    const EventDeviceInfo& devinfo,
    SharedPalmDetectionFilterState* shared_palm_state) {
  if (base::FeatureList::IsEnabled(kEnableNeuralPalmDetectionFilter) &&
      NeuralStylusPalmDetectionFilter::
          CompatibleWithNeuralStylusPalmDetectionFilter(devinfo)) {
    std::vector<float> radius_polynomial =
        internal::ParseRadiusPolynomial(kNeuralPalmRadiusPolynomial.Get());
    // Theres only one model right now.
    std::unique_ptr<NeuralStylusPalmDetectionFilterModel> model =
        std::make_unique<OneDeviceTrainNeuralStylusPalmDetectionFilterModel>(
            radius_polynomial);
    return std::make_unique<NeuralStylusPalmDetectionFilter>(
        devinfo, std::move(model), shared_palm_state);
  }

  if (base::FeatureList::IsEnabled(kEnableHeuristicPalmDetectionFilter)) {
    const base::TimeDelta hold_time =
        base::TimeDelta::FromSecondsD(kHeuristicHoldThresholdSeconds.Get());
    const base::TimeDelta cancel_time =
        base::TimeDelta::FromSecondsD(kHeuristicCancelThresholdSeconds.Get());
    const int stroke_count = kHeuristicStrokeCount.Get();
    return std::make_unique<HeuristicStylusPalmDetectionFilter>(
        shared_palm_state, stroke_count, hold_time, cancel_time);
  }
  return std::make_unique<OpenPalmDetectionFilter>(shared_palm_state);
}

}  // namespace ui
