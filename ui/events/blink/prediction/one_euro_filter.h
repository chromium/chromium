// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_PREDICTION_ONE_EURO_FILTER_H_
#define UI_EVENTS_BLINK_PREDICTION_ONE_EURO_FILTER_H_

#include "third_party/one_euro_filter/src/one_euro_filter.h"
#include "ui/events/blink/prediction/input_filter.h"

namespace ui {

// This class uses the 1€ filter from third party.
// See this page : http://cristal.univ-lille.fr/~casiez/1euro/
// to know how the filter works and how to tune it
class OneEuroFilter : public InputFilter {
 public:
  OneEuroFilter(double mincutoff = kDefaultMincutoff,
                double beta = kDefaultBeta);
  ~OneEuroFilter() override;

  bool Filter(const base::TimeTicks& timestamp,
              gfx::PointF* position) const override;

  const char* GetName() const override;

  InputFilter* Clone() override;

  void Reset() override;

  // Default parameters values for the filter
  static constexpr double kDefaultFrequency = 60;
  static constexpr double kDefaultMincutoff = 1.0;
  static constexpr double kDefaultBeta = 0.001;
  static constexpr double kDefaultDcutoff = 1.0;

  // Names of the fieldtrials used to tune the filter
  static constexpr char kParamBeta[] = "beta";
  static constexpr char kParamMincutoff[] = "mincutoff";

 private:
  std::unique_ptr<one_euro_filter::OneEuroFilter> x_filter_;
  std::unique_ptr<one_euro_filter::OneEuroFilter> y_filter_;

  DISALLOW_COPY_AND_ASSIGN(OneEuroFilter);
};

}  // namespace ui

#endif  // UI_EVENTS_BLINK_PREDICTION_ONE_EURO_FILTER_H_