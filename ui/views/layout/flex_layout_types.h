// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_FLEX_LAYOUT_TYPES_H_
#define UI_VIEWS_LAYOUT_FLEX_LAYOUT_TYPES_H_

#include <algorithm>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/views_export.h"

namespace gfx {
class Size;
}

namespace views {

class View;

// Specifies whether flex space is allocated in the same order as the children
// in the host view, or in reverse order. Reverse order is useful when you want
// child views to drop out from left to right instead of right to left if there
// isn't enough space to display them.
enum class FlexAllocationOrder { kNormal, kReverse };

// Callback used to specify the size of a child view based on its size bounds.
// Create your own custom rules, or use the Minimum|MaximumFlexSizeRule
// constants below for common behaviors.
//
// This callback takes two parameters: a child view, and a set of size bounds
// representing the available space for that child view to occupy. The function
// returns the preferred size of the view within those bounds, which may exceed
// them if the child is not capable of shrinking to the specified size. The
// callback may also return an empty size, which means the child view can drop
// out of the layout. Not specifying either bound means there is an unlimited
// amount of room for the child view in that dimension (and the child view
// should probably use its preferred size).
//
// We provide the ability to use an arbitrary function here because some views
// have complex sizing behavior; for example, they may shrink stepwise as their
// internal elements drop out due to lack of space.
using FlexRule =
    base::RepeatingCallback<gfx::Size(const View*, const SizeBounds&)>;

// Describes a simple rule for how a child view should shrink in a layout when
// the available size for that view decreases.
enum class MinimumFlexSizeRule {
  kScaleToZero,               // Ignore minimum size and scale all the way down.
  kScaleToMinimumSnapToZero,  // Scale to minimum then snap to zero.
  kPreferredSnapToZero,       // Use preferred, then snap to zero.
  kScaleToMinimum,            // Resize down to minimum then stop.
  kPreferredSnapToMinimum,    // Use preferred, then snap to minimum.
  kPreferred                  // Always use preferred size.
};

// Describes a simple rule for how a child view should grow in a layout when
// there is extra size available for that view to occupy.
enum class MaximumFlexSizeRule {
  kPreferred,       // Don't resize above preferred size.
  kScaleToMaximum,  // Allow resize up to the maximum size.
  kUnbounded        // Allow resize to arbitrary size.
};

// Specifies how a view should flex (i.e. grow or shrink) within its parent as
// the available space changes. Flex specifications have three components:
//  - A |rule| which tells the layout manager how the child view resizes with
//    available space.
//  - A |weight| which specifies how much each individual child will deviate
//    from its preferred size; larger weights will deviate more either when
//    shrinking or growing. The deviation is proportional to the weight divided
//    by the total weight of all views at this |order|.
//  - An |order| which specifies the priority with which available space is
//    allocated (lower numbers -> higher priority).
//
// Space allocation works as follows:
// 1. All views are given the smallest possible size that |rule| allows (which
//    might be zero.)
// 2. Going by |order|, we attempt to allocate the preferred size of each view.
// 3. If there is insufficient size, the deficit is allocated across all views
//    at this |order| whose |rule| allows them to shrink, proportional to
//    |weight|.
// 4. Once all orders have been allocated this way, repeat the process by
//    |order| allocating any excess space among views whose |rule| allows them
//    to exceed their preferred size.
//
// For example, say there are three child views in a horizontal layout, each
// of which has a flex rule that allows it to be between 10 and 40 DIP with a
// preferred width of 20 DIP. Child A is at order 2 with weight 2, child B is at
// order 1 with weight 1, and child C is at order 2 with weight 1.
//
// At 30 DIP (the parent's minimum size):
// [10][10][10]
//
// At 40 DIP:
// [10][20][10] (B hits its preferred size)
//
// At 48 DIP:
// [12][20][16] (deficit is spread across A and C by weight)
//
// At 57 DIP:
// [18][20][19]
//
// At 60 DIP:
// [20][20][20] (all views at preferred size)
//
// At 80 DIP:
// [20][40][20] (B hits its maximum size)
//
// At 110 DIP:
// [40][40][30] (A scales faster than C)
//
// At 120 DIP (the parent's maximum size):
// [40][40][40]
//
// NOTE(dfried): the behavior of |weight| may seem backwards when views shrink
// below their preferred size, but it works the way it does because:
//  (a) It's consistent with BoxLayout, making upgrading to FlexLayout easier.
//  (b) It allows smooth scaling across a view's preferred size.
// If this gets too confusing we could add an option to make weights reciprocal
// when allocating deficit.
class VIEWS_EXPORT FlexSpecification {
 public:
  // Creates a flex specification with the default rule (no flex, always use the
  // view's preferred size).
  FlexSpecification();

  // Creates a flex specification with a custom flex rule. Note that any copies
  // or mutations of this specification will also inherit the rule.
  explicit FlexSpecification(FlexRule rule);

  // Creates a flex specification using the specififed minimum size and size
  // bounds rules. If |adjust_height_for_width| is specified, extra calculations
  // will be done to ensure that the view can become taller if it is made
  // narrower (typically only useful for multiline text controls).
  //
  // NOTE: Minimum and maximum size rules apply to both main and cross axes of
  // the view in the layout. If you only need the view to flex based on its main
  // axis (width for horizontal layouts, height for vertical) consider using the
  // FlexSpecification(LayoutOrientation, ...) constructor below.
  explicit FlexSpecification(
      MinimumFlexSizeRule minimum_size_rule,
      MaximumFlexSizeRule maximum_size_rule = MaximumFlexSizeRule::kPreferred,
      bool adjust_height_for_width = false);

  // Creates a flex specification for a layout with |orientation| using the
  // given minimum and maximum flex size rules along the main axis. You may also
  // specify an optional cross-axis minimum size rule, but the default is to use
  // the child view's preferred size. (There is no max cross size rule because
  // unless a layout's cross-axis alignment is set to kStretch views will never
  // receive more than their preferred size in the cross-axis dimension.)
  FlexSpecification(LayoutOrientation orientation,
                    MinimumFlexSizeRule minimum_main_axis_rule,
                    MaximumFlexSizeRule maximum_main_axis_rule =
                        MaximumFlexSizeRule::kPreferred,
                    bool adjust_height_for_width = false,
                    MinimumFlexSizeRule minimum_cross_axis_rule =
                        MinimumFlexSizeRule::kPreferred);

  FlexSpecification(const FlexSpecification& other);
  FlexSpecification& operator=(const FlexSpecification& other);

  ~FlexSpecification();

  // Makes a copy of this specification with a different order.
  FlexSpecification WithOrder(int order) const;

  // Makes a copy of this specification with a different weight.
  // Specifying |weight| of zero means the view will take as much space as it
  // needs.
  FlexSpecification WithWeight(int weight) const;

  // Makes a copy of this specification with a different alignment. The default
  // is kStretch, which means the child view will always fill the bounds
  // allocated for it; specifying kLeading, kTrailing, or kCenter will cause the
  // view to grow to a maximum of its preferred size and then "float" to either
  // the center, leading, or trailing edge of the allocated space.
  FlexSpecification WithAlignment(LayoutAlignment alignment) const;

  const FlexRule& rule() const { return rule_; }
  int weight() const { return weight_; }
  int order() const { return order_; }
  LayoutAlignment alignment() const { return alignment_; }

 private:
  FlexRule rule_;
  int order_ = 1;
  int weight_ = 0;
  LayoutAlignment alignment_ = LayoutAlignment::kStretch;
};

// Represents insets in a single dimension.
class VIEWS_EXPORT Inset1D {
 public:
  constexpr Inset1D() = default;
  constexpr explicit Inset1D(int all) : leading_(all), trailing_(all) {}
  constexpr Inset1D(int leading, int trailing)
      : leading_(leading), trailing_(trailing) {}

  constexpr int leading() const { return leading_; }
  void set_leading(int leading) { leading_ = leading; }

  constexpr int trailing() const { return trailing_; }
  void set_trailing(int trailing) { trailing_ = trailing; }

  constexpr int size() const { return leading_ + trailing_; }

  void SetInsets(int leading, int trailing);
  void Expand(int delta_leading, int delta_trailing);

  constexpr bool is_empty() const { return leading_ == 0 && trailing_ == 0; }
  bool operator==(const Inset1D& other) const;
  bool operator!=(const Inset1D& other) const;
  bool operator<(const Inset1D& other) const;

  std::string ToString() const;

 private:
  int leading_ = 0;
  int trailing_ = 0;
};

// Represents a line segment in one dimension with a starting point and length.
class VIEWS_EXPORT Span {
 public:
  constexpr Span() = default;
  constexpr Span(int start, int length) : start_(start), length_(length) {}

  constexpr int start() const { return start_; }
  void set_start(int start) { start_ = start; }

  constexpr int length() const { return length_; }
  void set_length(int length) { length_ = std::max(0, length); }

  constexpr int end() const { return start_ + length_; }
  void set_end(int end) { set_length(end - start_); }

  void SetSpan(int start, int length);

  // Expands the span by |leading| at the front (reducing the value of start()
  // if |leading| is positive) and by |trailing| at the end (increasing the
  // value of end() if |trailing| is positive).
  void Expand(int leading, int trailing);

  // Opposite of Expand(). Shrinks each end of the span by the specified amount.
  void Inset(int leading, int trailing);
  void Inset(const Inset1D& insets);

  // Centers the span in another span, with optional margins.
  // Overflow is handled gracefully.
  void Center(const Span& container, const Inset1D& margins = Inset1D());

  // Aligns the span in another span, with optional margins, using the specified
  // alignment. Overflow is handled gracefully.
  void Align(const Span& container,
             LayoutAlignment alignment,
             const Inset1D& margins = Inset1D());

  constexpr bool is_empty() const { return length_ == 0; }
  bool operator==(const Span& other) const;
  bool operator!=(const Span& other) const;
  bool operator<(const Span& other) const;

  std::string ToString() const;

 private:
  int start_ = 0;
  int length_ = 0;
};

// These are declared here for use in gtest-based unit tests but is defined in
// the views_test_support target. Depend on that to use this in your unit test.
// This should not be used in production code - call ToString() instead.
void PrintTo(MinimumFlexSizeRule minimum_flex_size_rule, ::std::ostream* os);
void PrintTo(MaximumFlexSizeRule maximum_flex_size_rule, ::std::ostream* os);

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_FLEX_LAYOUT_TYPES_H_
