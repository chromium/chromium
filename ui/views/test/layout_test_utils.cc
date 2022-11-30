// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>

#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/layout_types.h"

namespace views {

void PrintTo(const SizeBounds& size_bounds, ::std::ostream* os) {
  *os << size_bounds.ToString();
}

void PrintTo(LayoutOrientation layout_orientation, ::std::ostream* os) {
  switch (layout_orientation) {
    case LayoutOrientation::kHorizontal:
      *os << "LayoutOrientation::kHorizontal";
      break;
    case LayoutOrientation::kVertical:
      *os << "LayoutOrientation::kVertical";
      break;
  }
}

void PrintTo(MinimumFlexSizeRule minimum_flex_size_rule, ::std::ostream* os) {
  switch (minimum_flex_size_rule) {
    case MinimumFlexSizeRule::kPreferred:
      *os << "MinimumFlexSizeRule::kPreferred";
      break;
    case MinimumFlexSizeRule::kPreferredSnapToMinimum:
      *os << "MinimumFlexSizeRule::kPreferredSnapToMinimum";
      break;
    case MinimumFlexSizeRule::kPreferredSnapToZero:
      *os << "MinimumFlexSizeRule::kPreferredSnapToZero";
      break;
    case MinimumFlexSizeRule::kScaleToMinimum:
      *os << "MinimumFlexSizeRule::kScaleToMinimum";
      break;
    case MinimumFlexSizeRule::kScaleToMinimumSnapToZero:
      *os << "MinimumFlexSizeRule::kScaleToMinimumSnapToZero";
      break;
    case MinimumFlexSizeRule::kScaleToZero:
      *os << "MinimumFlexSizeRule::kScaleToZero";
      break;
  }
}

void PrintTo(MaximumFlexSizeRule maximum_flex_size_rule, ::std::ostream* os) {
  switch (maximum_flex_size_rule) {
    case MaximumFlexSizeRule::kPreferred:
      *os << "MaximumFlexSizeRule::kPreferred";
      break;
    case MaximumFlexSizeRule::kScaleToMaximum:
      *os << "MaximumFlexSizeRule::kScaleToMaximum";
      break;
    case MaximumFlexSizeRule::kUnbounded:
      *os << "MaximumFlexSizeRule::kUnbounded";
      break;
  }
}

}  // namespace views
