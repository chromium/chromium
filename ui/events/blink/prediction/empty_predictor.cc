// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/prediction/empty_predictor.h"

namespace ui {

EmptyPredictor::EmptyPredictor() {
  Reset();
};

EmptyPredictor::~EmptyPredictor() = default;

const char* EmptyPredictor::GetName() const {
  return "Empty";
}

void EmptyPredictor::Reset() {
  last_input_.time_stamp = base::TimeTicks();
}

void EmptyPredictor::Update(const InputData& cur_input) {
  last_input_ = cur_input;
}

bool EmptyPredictor::HasPrediction() const {
  return false;
}

bool EmptyPredictor::GeneratePrediction(base::TimeTicks frame_time,
                                        InputData* result) const {
  if (!last_input_.time_stamp.is_null()) {
    result->pos = last_input_.pos;
    return true;
  }
  return false;
}

}  // namespace ui
