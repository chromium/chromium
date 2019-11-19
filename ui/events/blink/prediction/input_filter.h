// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_PREDICTION_INPUT_FILTER_H_
#define UI_EVENTS_BLINK_PREDICTION_INPUT_FILTER_H_

#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

// This class expects a sequence of inputs with coordinates and timestamps to
// return a smooth path from the sent coordinates.
class InputFilter {
 public:
  virtual ~InputFilter() = default;

  // Filters the position sent to the filter at a specific timestamp.
  // Returns true if the value is filtered, false otherwise.
  virtual bool Filter(const base::TimeTicks& timestamp,
                      gfx::PointF* position) const = 0;

  // Returns the name of the filter
  virtual const char* GetName() const = 0;

  // Returns a copy of the filter.
  virtual InputFilter* Clone() = 0;

  // Reset the filter to its initial state
  virtual void Reset() = 0;
};

}  // namespace ui

#endif  // UI_EVENTS_BLINK_PREDICTION_INPUT_FILTER_H_