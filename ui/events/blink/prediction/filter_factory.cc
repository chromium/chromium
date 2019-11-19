// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/prediction/filter_factory.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "ui/events/blink/prediction/empty_filter.h"
#include "ui/events/blink/prediction/one_euro_filter.h"
#include "ui/events/blink/prediction/predictor_factory.h"

namespace ui {

namespace input_prediction {

const char kFilterNameEmpty[] = "empty_filter";
const char kFilterNameOneEuro[] = "one_euro_filter";

}  // namespace input_prediction

namespace {
using input_prediction::FilterType;
using input_prediction::PredictorType;
}  // namespace

FilterFactory::FilterFactory(
    const base::Feature& feature,
    const input_prediction::PredictorType predictor_type,
    const input_prediction::FilterType filter_type) {
  LoadFilterParams(feature, predictor_type, filter_type);
}

FilterFactory::~FilterFactory() {}

void FilterFactory::LoadFilterParams(
    const base::Feature& feature,
    const input_prediction::PredictorType predictor_type,
    const input_prediction::FilterType filter_type) {
  if (filter_type == FilterType::kOneEuro) {
    base::FieldTrialParams one_euro_filter_param = {
        {OneEuroFilter::kParamBeta, ""}, {OneEuroFilter::kParamMincutoff, ""}};
    double beta, mincutoff;
    // Only save the params if they are given in the fieldtrials params
    if (base::GetFieldTrialParamsByFeature(feature, &one_euro_filter_param) &&
        base::StringToDouble(one_euro_filter_param[OneEuroFilter::kParamBeta],
                             &beta) &&
        base::StringToDouble(
            one_euro_filter_param[OneEuroFilter::kParamMincutoff],
            &mincutoff)) {
      FilterParamMapKey param_key = {FilterType::kOneEuro, predictor_type};
      FilterParams param_value = {{OneEuroFilter::kParamMincutoff, mincutoff},
                                  {OneEuroFilter::kParamBeta, beta}};
      filter_params_map_.emplace(param_key, param_value);
    }
  }
}

FilterType FilterFactory::GetFilterTypeFromName(
    const std::string& filter_name) {
  if (filter_name == input_prediction::kFilterNameOneEuro)
    return FilterType::kOneEuro;
  else
    return FilterType::kEmpty;
}

std::unique_ptr<InputFilter> FilterFactory::CreateFilter(
    const FilterType filter_type,
    const PredictorType predictor_type) {
  FilterParams filter_params;
  GetFilterParams(filter_type, predictor_type, &filter_params);
  if (filter_type == FilterType::kOneEuro) {
    if (filter_params.empty())
      return std::make_unique<OneEuroFilter>();
    else
      return std::make_unique<OneEuroFilter>(
          filter_params.find(OneEuroFilter::kParamMincutoff)->second,
          filter_params.find(OneEuroFilter::kParamBeta)->second);
  } else
    return std::make_unique<EmptyFilter>();
}

void FilterFactory::GetFilterParams(const FilterType filter_type,
                                    const PredictorType predictor_type,
                                    FilterParams* filter_params) {
  FilterParamMapKey key = {filter_type, predictor_type};
  auto params = filter_params_map_.find(key);
  if (params != filter_params_map_.end()) {
    *filter_params = params->second;
  }
}

}  // namespace ui
