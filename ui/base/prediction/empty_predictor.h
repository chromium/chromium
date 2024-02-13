// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_PREDICTION_EMPTY_PREDICTOR_H_
#define UI_BASE_PREDICTION_EMPTY_PREDICTOR_H_

#include <optional>

#include "base/component_export.h"
#include "ui/base/prediction/input_predictor.h"

namespace ui {

// An empty predictor class. This will not generate any prediction.
class COMPONENT_EXPORT(UI_BASE_PREDICTION) EmptyPredictor
    : public InputPredictor {
 public:
  EmptyPredictor();

  EmptyPredictor(const EmptyPredictor&) = delete;
  EmptyPredictor& operator=(const EmptyPredictor&) = delete;

  ~EmptyPredictor() override;

  const char* GetName() const override;

  void Reset() override;

  // store the cur_input in last_input_
  void Update(const InputData& cur_input) override;

  // Always returns false;
  bool HasPrediction() const override;

  // Returns the last_input_ for testing.
  std::unique_ptr<InputData> GeneratePrediction(
      base::TimeTicks predict_time,
      base::TimeDelta frame_interval) override;

  // Returns kTimeInterval for testing.
  base::TimeDelta TimeInterval() const override;

 private:
  // store the last_input_ point for testing
  std::optional<InputData> last_input_;
};

}  // namespace ui

#endif  // UI_BASE_PREDICTION_EMPTY_PREDICTOR_H_
