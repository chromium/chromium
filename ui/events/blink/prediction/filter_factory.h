// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_PREDICTION_FILTER_FACTORY_H_
#define UI_EVENTS_BLINK_PREDICTION_FILTER_FACTORY_H_

#include "base/feature_list.h"
#include "ui/events/blink/prediction/input_filter.h"
#include "ui/events/blink/prediction/predictor_factory.h"

namespace ui {

namespace test {
class FilterFactoryTest;
}  // namespace test

namespace input_prediction {
extern const char kFilterNameEmpty[];
extern const char kFilterNameOneEuro[];

enum class FilterType {
  kEmpty,
  kOneEuro,
};
}  // namespace input_prediction

// Structure used as key in the unordered_map to store different filter params
// in function of a trio {Filter, Predictor, Feature}
struct FilterParamMapKey {
  bool operator==(const FilterParamMapKey& other) const {
    return filter_type == other.filter_type &&
           predictor_type == other.predictor_type;
  }
  input_prediction::FilterType filter_type;
  input_prediction::PredictorType predictor_type;
};

// Used to compute a hash value for FilterParamMapKey so it can be used as key
// in a hashmap
struct FilterParamMapKeyHash {
  std::size_t operator()(const FilterParamMapKey& k) const {
    return std::hash<input_prediction::FilterType>{}(k.filter_type) ^
           std::hash<input_prediction::PredictorType>{}(k.predictor_type);
  }
};

using FilterParams = std::unordered_map<std::string, double>;
using FilterParamsMap =
    std::unordered_map<FilterParamMapKey, FilterParams, FilterParamMapKeyHash>;

// FilterFactory is a class containing methods to create filters.
// It defines filters name and type constants. It also reads filter settings
// from fieldtrials if needed.
class FilterFactory {
 public:
  FilterFactory(const base::Feature& feature,
                const input_prediction::PredictorType predictor_type,
                const input_prediction::FilterType filter_type);
  ~FilterFactory();

  // Returns the FilterType associated to the given filter
  // name if found, otherwise returns kFilterTypeEmpty
  input_prediction::FilterType GetFilterTypeFromName(
      const std::string& filter_name);

  // Returns the filter designed by its type.
  // Since filters can have different parameters in function of the current
  // predictor used, it also needs to be given as parameter.
  std::unique_ptr<InputFilter> CreateFilter(
      const input_prediction::FilterType filter_type,
      const input_prediction::PredictorType predictor_type);

 private:
  friend class test::FilterFactoryTest;

  // Map storing filter parameters for a pair {FilterType, PredictorType}.
  // Currently the map is only storing values from fieldtrials params, but
  // default parameters might be added for a specific predictor/filter pair
  // in the future.
  FilterParamsMap filter_params_map_;

  // Initialize the filter_params_map_ with values from fieldtrials params for
  // a given feature, predictor and filter.
  // Might initialize some default values for specific predictor/filter pair in
  // the future.
  void LoadFilterParams(const base::Feature& feature,
                        const input_prediction::PredictorType predictor_type,
                        const input_prediction::FilterType filter_type);

  // Gets filter params for a specific key couple {FilterType, PredictorType}
  // If params are found, update the given filter_params map.
  void GetFilterParams(const input_prediction::FilterType filter_type,
                       const input_prediction::PredictorType predictor_type,
                       FilterParams* filter_params);
};
}  // namespace ui

#endif  // UI_EVENTS_BLINK_PREDICTION_FILTER_FACTORY_H_