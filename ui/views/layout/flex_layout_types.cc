// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/layout/flex_layout_types.h"

#include <algorithm>
#include <tuple>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view.h"

namespace views {

namespace {

// Default Flex Rules ----------------------------------------------------------

constexpr MaximumFlexSizeRule kDefaultMaximumFlexSizeRule =
    MaximumFlexSizeRule::kPreferred;

class LazySize;

// Helper object that lazily returns either the width or height of a LazySize
// (see below).
class LazyDimension {
 public:
  LazyDimension(const LazySize* size, LayoutOrientation dimension)
      : size_(size), dimension_(dimension) {}

  int operator*() const { return get(); }
  int get() const;

 private:
  const raw_ptr<const LazySize> size_;
  LayoutOrientation dimension_;
};

// Some of a view's sizing methods can be expensive to compute. This provides
// a lazy-eval value that behaves like a smart pointer but is more lightweight
// than base::LazyInstance.
class LazySize {
 public:
  using SizeFunc = gfx::Size (View::*)() const;

  explicit LazySize(const View* view, SizeFunc size_func)
      : view_(view), size_func_(size_func) {}
  LazySize(const LazySize& other) = default;
  ~LazySize() = default;
  // Note: copy operator is implicitly deleted due to const data member.

  const gfx::Size* operator->() const { return get(); }
  const gfx::Size& operator*() const { return *get(); }
  const gfx::Size* get() const {
    if (!size_)
      size_ = (view_->*size_func_)();
    return &size_.value();
  }
  LazyDimension width() const {
    return LazyDimension(this, LayoutOrientation::kHorizontal);
  }
  LazyDimension height() const {
    return LazyDimension(this, LayoutOrientation::kVertical);
  }

 private:
  const raw_ptr<const View> view_;
  SizeFunc size_func_;
  mutable std::optional<gfx::Size> size_;
};

int LazyDimension::get() const {
  switch (dimension_) {
    case LayoutOrientation::kHorizontal:
      return (*size_)->width();
    case LayoutOrientation::kVertical:
      return (*size_)->height();
  }
}

// Interpolates a size between minimum, preferred size, and upper bound based on
// sizing rules, returning the resulting ideal size.
int InterpolateSize(MinimumFlexSizeRule minimum_size_rule,
                    MaximumFlexSizeRule maximum_size_rule,
                    int minimum_size,
                    int preferred_size,
                    LazyDimension maximum_size,
                    int available_size) {
  // A view may (mistakenly) report a minimum size larger than its preferred
  // size. While in principle this shouldn't happen, by the time we've gotten
  // here it's better to simply make sure the minimum and preferred don't
  // cross.
  minimum_size = std::min(minimum_size, preferred_size);

  // TODO(dfried): this could be rearranged to allow lazy evaluation of
  // minimum_size.
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
  }
  if (available_size < preferred_size) {
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
  }
  switch (maximum_size_rule) {
    case MaximumFlexSizeRule::kPreferred:
      return preferred_size;
    case MaximumFlexSizeRule::kScaleToMaximum: {
      // A view may (mistakenly) report a maximum size smaller than its
      // preferred size. While in principle this shouldn't happen, by the
      // time we're here it's better to simply make sure the maximum and
      // preferred size don't cross.
      const int actual_maximum_size = std::max(preferred_size, *maximum_size);
      return std::min(available_size, actual_maximum_size);
    }
    case MaximumFlexSizeRule::kUnbounded:
      return available_size;
  }
}

gfx::Size GetPreferredSize(MinimumFlexSizeRule minimum_width_rule,
                           MaximumFlexSizeRule maximum_width_rule,
                           MinimumFlexSizeRule minimum_height_rule,
                           MaximumFlexSizeRule maximum_height_rule,
                           bool adjust_height_for_width,
                           const View* view,
                           const SizeBounds& size_bounds) {
  LazySize minimum_size(view, &View::GetMinimumSize);
  LazySize maximum_size(view, &View::GetMaximumSize);
  gfx::Size preferred = view->GetPreferredSize(size_bounds);

  int width;
  if (!size_bounds.width().is_bounded()) {
    // Not having a maximum size is different from having a large available
    // size; a view can't grow infinitely, so we go with its preferred size.
    width = preferred.width();
  } else {
    width = InterpolateSize(minimum_width_rule, maximum_width_rule,
                            minimum_size->width(), preferred.width(),
                            maximum_size.width(), size_bounds.width().value());
  }

  int preferred_height = preferred.height();
  if (adjust_height_for_width) {
    // The |adjust_height_for_width| flag is used in vertical layouts where we
    // want views to be able to adapt to the horizontal available space by
    // growing vertically. We therefore allow the horizontal size to shrink even
    // if there's otherwise no flex allowed.
    if (size_bounds.width() > 0)
      width = size_bounds.width().min_of(width);

    if (width < preferred.width()) {
      // Allow views that need to grow vertically when they're compressed
      // horizontally to do so.
      //
      // If we just went with GetHeightForWidth() we would have situations where
      // an empty text control wanted no (or very little) height which could
      // cause a layout to shrink vertically; we will always try to allocate at
      // least the view's reported preferred height.
      //
      // Note that this is an adjustment made for practical considerations, and
      // may not be "correct" in some absolute sense. Let's revisit at some
      // point.
      preferred_height =
          std::max(preferred_height, view->GetHeightForWidth(width));
    }
  }

  int height;
  if (!size_bounds.height().is_bounded()) {
    // Not having a maximum size is different from having a large available
    // size; a view can't grow infinitely, so we go with its preferred size.
    height = preferred_height;
  } else {
    height = InterpolateSize(
        minimum_height_rule, maximum_height_rule, minimum_size->height(),
        preferred_height, maximum_size.height(), size_bounds.height().value());
  }

  return gfx::Size(width, height);
}

}  // namespace

// FlexSpecification -----------------------------------------------------------

FlexSpecification::FlexSpecification()
    : rule_(base::BindRepeating(&GetPreferredSize,
                                MinimumFlexSizeRule::kPreferred,
                                MaximumFlexSizeRule::kPreferred,
                                MinimumFlexSizeRule::kPreferred,
                                MaximumFlexSizeRule::kPreferred,
                                false)) {}

FlexSpecification::FlexSpecification(FlexRule rule)
    : rule_(std::move(rule)), weight_(1) {}

FlexSpecification::FlexSpecification(MinimumFlexSizeRule minimum_size_rule,
                                     MaximumFlexSizeRule maximum_size_rule,
                                     bool adjust_height_for_width)
    : FlexSpecification(base::BindRepeating(&GetPreferredSize,
                                            minimum_size_rule,
                                            maximum_size_rule,
                                            minimum_size_rule,
                                            maximum_size_rule,
                                            adjust_height_for_width)) {}

FlexSpecification::FlexSpecification(
    LayoutOrientation orientation,
    MinimumFlexSizeRule minimum_main_axis_rule,
    MaximumFlexSizeRule maximum_main_axis_rule,
    bool adjust_height_for_width,
    MinimumFlexSizeRule minimum_cross_axis_rule)
    : FlexSpecification(base::BindRepeating(
          &GetPreferredSize,
          orientation == LayoutOrientation::kHorizontal
              ? minimum_main_axis_rule
              : minimum_cross_axis_rule,
          orientation == LayoutOrientation::kHorizontal
              ? maximum_main_axis_rule
              : kDefaultMaximumFlexSizeRule,
          orientation == LayoutOrientation::kVertical ? minimum_main_axis_rule
                                                      : minimum_cross_axis_rule,
          orientation == LayoutOrientation::kVertical
              ? maximum_main_axis_rule
              : kDefaultMaximumFlexSizeRule,
          adjust_height_for_width)) {}

FlexSpecification::FlexSpecification(const FlexSpecification& other) = default;

FlexSpecification& FlexSpecification::operator=(
    const FlexSpecification& other) = default;

FlexSpecification::~FlexSpecification() = default;

FlexSpecification FlexSpecification::WithWeight(int weight) const {
  DCHECK_GE(weight, 0);
  FlexSpecification spec = *this;
  spec.weight_ = weight;
  return spec;
}

FlexSpecification FlexSpecification::WithOrder(int order) const {
  DCHECK_GE(order, 1);
  FlexSpecification spec = *this;
  spec.order_ = order;
  return spec;
}

FlexSpecification FlexSpecification::WithAlignment(
    LayoutAlignment alignment) const {
  FlexSpecification spec = *this;
  spec.alignment_ = alignment;
  return spec;
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
  return std::tie(leading_, trailing_) ==
         std::tie(other.leading_, other.trailing_);
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
    set_start(container.start() + base::ClampCeil(remaining * 0.5f));
    return;
  }

  // Case 2: room for only part of the margins.
  if (margins.size() > remaining) {
    float scale = static_cast<float>(remaining) / margins.size();
    set_start(container.start() + base::ClampRound(scale * margins.leading()));
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
    case LayoutAlignment::kBaseline:
      NOTIMPLEMENTED();
      break;
  }
}

bool Span::operator==(const Span& other) const {
  return std::tie(start_, length_) == std::tie(other.start_, other.length_);
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
