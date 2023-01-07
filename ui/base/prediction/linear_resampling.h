// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_PREDICTION_LINEAR_RESAMPLING_H_
#define UI_BASE_PREDICTION_LINEAR_RESAMPLING_H_

#include <deque>

#include "base/component_export.h"
#include "base/time/time.h"
#include "ui/base/prediction/input_predictor.h"

namespace ui {

// This class use linear extrapolates / interpolates to resample events to
// frame_time - kResampleLatency. This resampling logic is to match the
// resampling behavior on Android. It's not designed for pointerevent's
// PredictedEvent and should not be used for that purpose.
// Resampling on Android see:
// https://android.googlesource.com/platform/frameworks/native/+/master/libs/input/InputTransport.cpp
class COMPONENT_EXPORT(UI_BASE_PREDICTION) LinearResampling
    : public InputPredictor {
 public:
  explicit LinearResampling();

  LinearResampling(const LinearResampling&) = delete;
  LinearResampling& operator=(const LinearResampling&) = delete;

  ~LinearResampling() override;

  const char* GetName() const override;

  // Reset the predictor to initial state.
  void Reset() override;

  // Store current input in queue.
  void Update(const InputData& new_input) override;

  // Return if there is enough data in the queue to generate prediction.
  bool HasPrediction() const override;

  // Generate the prediction based on stored points and given frame_time.
  // Return false if no prediction available.
  std::unique_ptr<InputData> GeneratePrediction(
      base::TimeTicks frame_time,
      base::TimeDelta frame_interval) override;

  // Return the average time delta in the event queue.
  base::TimeDelta TimeInterval() const override;

 private:
  // Class to cache the Resample Latency to avoid its recalculation each frame.
  class LatencyCalculator {
   public:
    base::TimeDelta GetResampleLatency(base::TimeDelta frame_interval);

   private:
    base::TimeDelta CalculateLatency();
    base::TimeDelta resample_latency_;
    base::TimeDelta frame_interval_;
  };

  static constexpr size_t kNumEventsForResampling = 2;

  // Store the last events received
  std::deque<InputData> events_queue_;

  // Store the current delta time between the last 2 events
  base::TimeDelta events_dt_;

  LatencyCalculator latency_calculator_;
};

}  // namespace ui

#endif  // UI_BASE_PREDICTION_LINEAR_RESAMPLING_H_
