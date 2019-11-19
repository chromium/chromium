// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_PREDICTION_INPUT_PREDICTOR_H_
#define UI_EVENTS_BLINK_PREDICTION_INPUT_PREDICTOR_H_

#include <memory>

#include "base/macros.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

// This class expects a sequence of inputs with their coordinates and timestamps
// and models the input path. It then can predict the coordinates at any given
// time.
class InputPredictor {
 public:
  virtual ~InputPredictor() = default;

  struct InputData {
    gfx::PointF pos;
    base::TimeTicks time_stamp;
    InputData() {
      pos = gfx::PointF();
      time_stamp = base::TimeTicks();
    }
    InputData(const gfx::PointF& event_pos, const base::TimeTicks& event_time) {
      pos = event_pos;
      time_stamp = event_time;
    }
  };

  // Returns the name of the predictor.
  virtual const char* GetName() const = 0;

  // Reset should be called each time when a new line start.
  virtual void Reset() = 0;

  // Update the predictor with new input points.
  virtual void Update(const InputData& new_input) = 0;

  // Return true if the predictor is able to predict points.
  virtual bool HasPrediction() const = 0;

  // Generate the prediction based on current points.
  virtual std::unique_ptr<InputData> GeneratePrediction(
      base::TimeTicks predict_time) const = 0;

  // Returns the maximum of prediction available for resampling
  // before having side effects (jitter, wrong orientation, etc..)
  const base::TimeDelta MaxResampleTime() const { return kMaxResampleTime; }

  // Returns the maximum prediction time available for the predictor
  // before having side effects (jitter, wrong orientation, etc..)
  const base::TimeDelta MaxPredictionTime() const { return kMaxPredictionTime; }

  // Return the time interval based on current points.
  virtual base::TimeDelta TimeInterval() const = 0;

 protected:
  static constexpr base::TimeDelta kMaxTimeDelta =
      base::TimeDelta::FromMilliseconds(20);

  // Default time interval between events.
  static constexpr base::TimeDelta kTimeInterval =
      base::TimeDelta::FromMilliseconds(8);
  // Minimum time interval between events.
  static constexpr base::TimeDelta kMinTimeInterval =
      base::TimeDelta::FromMillisecondsD(2.5);

  // Maximum amount of prediction when resampling.
  static constexpr base::TimeDelta kMaxResampleTime =
      base::TimeDelta::FromMilliseconds(20);
  // Maximum time delta for prediction.
  static constexpr base::TimeDelta kMaxPredictionTime =
      base::TimeDelta::FromMilliseconds(25);
};

}  // namespace ui

#endif  // UI_EVENTS_BLINK_PREDICTION_INPUT_PREDICTOR_H_
