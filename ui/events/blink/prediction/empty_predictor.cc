// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/prediction/empty_predictor.h"
#include "ui/events/blink/prediction/predictor_factory.h"

namespace ui {

EmptyPredictor::EmptyPredictor() {
  Reset();
}

EmptyPredictor::~EmptyPredictor() = default;

const char* EmptyPredictor::GetName() const {
  return input_prediction::kScrollPredictorNameEmpty;
}

void EmptyPredictor::Reset() {
  last_input_ = base::nullopt;
}

void EmptyPredictor::Update(const InputData& cur_input) {
  last_input_ = cur_input;
}

bool EmptyPredictor::HasPrediction() const {
  return last_input_.has_value();
}

std::unique_ptr<InputPredictor::InputData> EmptyPredictor::GeneratePrediction(
    base::TimeTicks predict_time) const {
  if (!HasPrediction())
    return nullptr;
  return std::make_unique<InputData>(last_input_.value());
}

base::TimeDelta EmptyPredictor::TimeInterval() const {
  return kTimeInterval;
}

}  // namespace ui
