// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/flex_layout_types.h"

#include <algorithm>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

namespace views {

namespace {

// Default Flex Rules ----------------------------------------------------------

// Interpolates a size between minimum, preferred size, and upper bound based on
// sizing rules, returning the resulting ideal size.
int InterpolateSize(MinimumFlexSizeRule minimum_size_rule,
                    MaximumFlexSizeRule maximum_size_rule,
                    int minimum_size,
                    int preferred_size,
                    int available_size) {
  if (available_size < minimum_size) {
    switch (minimum_size_rule) {
      case MinimumFlexSizeRule::kScaleToZero:
        return available_size;
      case MinimumFlexSizeRule::kPreferred:
        return preferred_size;
      case MinimumFlexSizeRule::kScaleToMinimum:
      case MinimumFlexSizeRule::kPreferredSnapToMinimum:
        return minimum_size;
      case MinimumFlexSizeRule::kScaleToMinimumSnapToZero:
      case MinimumFlexSizeRule::kPreferredSnapToZero:
        return 0;
    }
  } else if (available_size < preferred_size) {
    switch (minimum_size_rule) {
      case MinimumFlexSizeRule::kPreferred:
        return preferred_size;
      case MinimumFlexSizeRule::kScaleToZero:
      case MinimumFlexSizeRule::kScaleToMinimum:
      case MinimumFlexSizeRule::kScaleToMinimumSnapToZero:
        return available_size;
      case MinimumFlexSizeRule::kPreferredSnapToMinimum:
        return minimum_size;
      case MinimumFlexSizeRule::kPreferredSnapToZero:
        return 0;
    }
  } else {
    switch (maximum_size_rule) {
      case MaximumFlexSizeRule::kPreferred:
        return preferred_size;
      case MaximumFlexSizeRule::kUnbounded:
        return available_size;
    }
  }
}

gfx::Size GetPreferredSize(MinimumFlexSizeRule minimum_size_rule,
                           MaximumFlexSizeRule maximum_size_rule,
                           bool adjust_height_for_width,
                           const views::View* view,
                           const views::SizeBounds& maximum_size) {
  gfx::Size min = view->GetMinimumSize();
  gfx::Size preferred = view->GetPreferredSize();

  int width, height;

  if (!maximum_size.width()) {
    // Not having a maximum size is different from having a large available
    // size; a view can't grow infinitely, so we go with its preferred size.
    width = preferred.width();
  } else {
    width = InterpolateSize(minimum_size_rule, maximum_size_rule, min.width(),
                            preferred.width(), *maximum_size.width());
  }

  int preferred_height = preferred.height();
  if (adjust_height_for_width && width < preferred.width()) {
    // Allow views that need to grow vertically when they're compressed
    // horizontally to do so.
    //
    // If we just went with GetHeightForWidth() we would have situations where
    // an empty text control wanted no (or very little) height which could cause
    // a layout to shrink vertically; we will always try to allocate at least
    // the view's reported preferred height.
    //
    // Note that this is an adjustment made for practical considerations, and
    // may not be "correct" in some absolute sense. Let's revisit at some point.
    preferred_height =
        std::max(preferred_height, view->GetHeightForWidth(width));
  }

  if (!maximum_size.height()) {
    // Not having a maximum size is different from having a large available
    // size; a view can't grow infinitely, so we go with its preferred size.
    height = preferred_height;
  } else {
    height = InterpolateSize(minimum_size_rule, maximum_size_rule, min.height(),
                             preferred_height, *maximum_size.height());
  }

  return gfx::Size(width, height);
}

FlexRule GetDefaultFlexRule(
    MinimumFlexSizeRule minimum_size_rule = MinimumFlexSizeRule::kPreferred,
    MaximumFlexSizeRule maximum_size_rule = MaximumFlexSizeRule::kPreferred,
    bool adjust_height_for_width = false) {
  return base::BindRepeating(&GetPreferredSize, minimum_size_rule,
                             maximum_size_rule, adjust_height_for_width);
}

}  // namespace

// FlexSpecification -----------------------------------------------------------

FlexSpecification::FlexSpecification() : rule_(GetDefaultFlexRule()) {}

FlexSpecification FlexSpecification::ForCustomRule(FlexRule rule) {
  return FlexSpecification(std::move(rule), 1, 1, LayoutAlignment::kStretch);
}

FlexSpecification FlexSpecification::ForSizeRule(
    MinimumFlexSizeRule minimum_size_rule,
    MaximumFlexSizeRule maximum_size_rule,
    bool adjust_height_for_width) {
  return FlexSpecification(
      GetDefaultFlexRule(minimum_size_rule, maximum_size_rule,
                         adjust_height_for_width),
      1, 1, LayoutAlignment::kStretch);
}

FlexSpecification::FlexSpecification(FlexRule rule,
                                     int order,
                                     int weight,
                                     LayoutAlignment alignment)
    : rule_(std::move(rule)),
      order_(order),
      weight_(weight),
      alignment_(alignment) {}

FlexSpecification::FlexSpecification(const FlexSpecification& other) = default;

FlexSpecification& FlexSpecification::operator=(
    const FlexSpecification& other) = default;

FlexSpecification::~FlexSpecification() = default;

FlexSpecification FlexSpecification::WithWeight(int weight) const {
  DCHECK_GE(weight, 0);
  return FlexSpecification(rule_, order_, weight, alignment_);
}

FlexSpecification FlexSpecification::WithOrder(int order) const {
  DCHECK_GE(order, 1);
  return FlexSpecification(rule_, order, weight_, alignment_);
}

FlexSpecification FlexSpecification::WithAlignment(
    LayoutAlignment alignment) const {
  return FlexSpecification(rule_, order_, weight_, alignment);
}

// Inset1D ---------------------------------------------------------------------

void Inset1D::SetInsets(int leading, int trailing) {
  leading_ = leading;
  trailing_ = trailing;
}

void Inset1D::Expand(int leading, int trailing) {
  leading_ += leading;
  trailing_ += trailing;
}

bool Inset1D::operator==(const Inset1D& other) const {
  return leading_ == other.leading_ && trailing_ == other.trailing_;
}

bool Inset1D::operator!=(const Inset1D& other) const {
  return !(*this == other);
}

bool Inset1D::operator<(const Inset1D& other) const {
  return std::tie(leading_, trailing_) <
         std::tie(other.leading_, other.trailing_);
}

std::string Inset1D::ToString() const {
  return base::StringPrintf("%d, %d", leading(), trailing());
}

// Span ------------------------------------------------------------------------

void Span::SetSpan(int start, int length) {
  start_ = start;
  length_ = std::max(0, length);
}

void Span::Expand(int leading, int trailing) {
  const int end = this->end();
  set_start(start_ - leading);
  set_end(end + trailing);
}

void Span::Inset(int leading, int trailing) {
  Expand(-leading, -trailing);
}

void Span::Inset(const Inset1D& insets) {
  Inset(insets.leading(), insets.trailing());
}

void Span::Center(const Span& container, const Inset1D& margins) {
  int remaining = container.length() - length();

  // Case 1: no room for any margins. Just center the span in the container,
  // with equal overflow on each side.
  if (remaining <= 0) {
    set_start(container.start() + std::ceil(remaining * 0.5f));
    return;
  }

  // Case 2: room for only part of the margins.
  if (margins.size() > remaining) {
    float scale = float{remaining} / float{margins.size()};
    set_start(container.start() + std::roundf(scale * margins.leading()));
    return;
  }

  // Case 3: room for both span and margins. Center the whole unit.
  remaining -= margins.size();
  set_start(container.start() + remaining / 2 + margins.leading());
}

void Span::Align(const Span& container,
                 LayoutAlignment alignment,
                 const Inset1D& margins) {
  switch (alignment) {
    case LayoutAlignment::kStart:
      set_start(container.start() + margins.leading());
      break;
    case LayoutAlignment::kEnd:
      set_start(container.end() - (margins.trailing() + length()));
      break;
    case LayoutAlignment::kCenter:
      Center(container, margins);
      break;
    case LayoutAlignment::kStretch:
      SetSpan(container.start() + margins.leading(),
              std::max(0, container.length() - margins.size()));
      break;
  }
}

bool Span::operator==(const Span& other) const {
  return start_ == other.start_ && length_ == other.length_;
}

bool Span::operator!=(const Span& other) const {
  return !(*this == other);
}

bool Span::operator<(const Span& other) const {
  return std::tie(start_, length_) < std::tie(other.start_, other.length_);
}

std::string Span::ToString() const {
  return base::StringPrintf("%d [%d]", start(), length());
}

}  // namespace views
