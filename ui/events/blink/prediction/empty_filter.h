// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_BLINK_PREDICTION_EMPTY_FILTER_H_
#define UI_EVENTS_BLINK_PREDICTION_EMPTY_FILTER_H_

#include "ui/events/blink/prediction/input_filter.h"

namespace ui {

// Empty filter is a fake filter. Always returns the same input position as
// the filtered position. Mainly used for testing purpose.
class EmptyFilter : public InputFilter {
 public:
  explicit EmptyFilter();
  ~EmptyFilter() override;

  // Filters the position sent to the filter at a specific timestamp.
  // Returns true if the value is filtered, false otherwise.
  bool Filter(const base::TimeTicks& timestamp,
              gfx::PointF* position) const override;

  // Returns the name of the filter
  const char* GetName() const override;

  // Returns a copy of the filter.
  InputFilter* Clone() override;

  // Reset the filter to its initial state
  void Reset() override;

  DISALLOW_COPY_AND_ASSIGN(EmptyFilter);
};

}  // namespace ui

#endif  // UI_EVENTS_BLINK_PREDICTION_EMPTY_FILTER_H_
