// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_PREDICTION_INPUT_FILTER_H_
#define UI_BASE_PREDICTION_INPUT_FILTER_H_

#include "base/component_export.h"
#include "base/time/time.h"
#include "ui/gfx/geometry/point_f.h"

namespace ui {

// This class expects a sequence of inputs with coordinates and timestamps to
// return a smooth path from the sent coordinates.
class COMPONENT_EXPORT(UI_BASE_PREDICTION) InputFilter {
 public:
  virtual ~InputFilter() = default;

  // Filters the position sent to the filter at a specific timestamp.
  // Returns true if the value is filtered, false otherwise.
  virtual bool Filter(const base::TimeTicks& timestamp,
                      gfx::PointF* position) const = 0;

  // Returns the name of the filter
  virtual const char* GetName() const = 0;
};

}  // namespace ui

#endif  // UI_BASE_PREDICTION_INPUT_FILTER_H_
