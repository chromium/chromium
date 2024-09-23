// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_PREDICTION_EMPTY_FILTER_H_
#define UI_BASE_PREDICTION_EMPTY_FILTER_H_

#include "base/component_export.h"
#include "ui/base/prediction/input_filter.h"

namespace ui {

// Empty filter is a fake filter. Always returns the same input position as
// the filtered position. Mainly used for testing purpose.
class COMPONENT_EXPORT(UI_BASE_PREDICTION) EmptyFilter : public InputFilter {
 public:
  explicit EmptyFilter();

  EmptyFilter(const EmptyFilter&) = delete;
  EmptyFilter& operator=(const EmptyFilter&) = delete;

  ~EmptyFilter() override;

  // Filters the position sent to the filter at a specific timestamp.
  // Returns true if the value is filtered, false otherwise.
  bool Filter(const base::TimeTicks& timestamp,
              gfx::PointF* position) const override;

  // Returns the name of the filter
  const char* GetName() const override;
};

}  // namespace ui

#endif  // UI_BASE_PREDICTION_EMPTY_FILTER_H_
